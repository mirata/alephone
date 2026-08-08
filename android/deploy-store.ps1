# Upload the three branded Quest apps (Marathon / Marathon 2 / Infinity) to the Meta Horizon
# Store via ovr-platform-util. See [[quest-store-distribution]] / [[dev-workflow]].
#
# Usage:
#   pwsh android\deploy-store.ps1 -Notes "3D weapons (0.8)"     # build + upload all 3 to ALPHA
#   pwsh android\deploy-store.ps1 -Notes "..." -SkipBuild        # upload the existing APKs, no rebuild
#   pwsh android\deploy-store.ps1 -Notes "..." -Apps marathon2   # just one app (repeatable: -Apps marathon,infinity)
#   pwsh android\deploy-store.ps1 -Notes "..." -Channel BETA -DryRun
#
# ONE-TIME SETUP: copy store.properties.template -> store.properties and fill in the three
# App Secrets (Meta Developer Dashboard -> app -> API). store.properties is gitignored.
#
# REMINDER: bump versionCode in android/app/build.gradle BEFORE running this — Meta rejects any
# upload whose versionCode isn't strictly higher than the last one on that listing. This script
# does NOT bump it; it uploads whatever the built APKs contain (and prints each versionCode).

param(
    [string]$Notes,
    [string[]]$Apps = @("marathon", "marathon2", "infinity"),
    [string]$Channel = "ALPHA",
    [string]$AgeGroup = "TEENS_AND_ADULTS",
    [switch]$SkipBuild,
    [switch]$DryRun,
    [string]$OvrUtil = "$env:APPDATA\metavr\tools\platform-utils\ovr-platform-util.exe"
)
$ErrorActionPreference = "Stop"

$repo    = Split-Path -Parent $PSScriptRoot         # android/ is directly under the repo root
$android = Join-Path $repo "android"
$apkRoot = Join-Path $android "app\build\outputs\apk"

# App IDs are NOT secret (baked in). App Secrets come from store.properties (gitignored).
$catalog = [ordered]@{
    marathon  = @{ AppId = "1916477862362986"; Flavor = "Marathon";  Apk = "$apkRoot\marathon\release\app-marathon-release.apk" }
    marathon2 = @{ AppId = "1903718786972227"; Flavor = "Marathon2"; Apk = "$apkRoot\marathon2\release\app-marathon2-release.apk" }
    infinity  = @{ AppId = "1916480205696085"; Flavor = "Infinity";  Apk = "$apkRoot\infinity\release\app-infinity-release.apk" }
}

# --- Validate requested apps -------------------------------------------------
foreach ($a in $Apps) {
    if (-not $catalog.Contains($a)) {
        throw "Unknown app '$a'. Valid: $($catalog.Keys -join ', ')"
    }
}

if (-not $Notes) {
    throw "-Notes is required (release-note text shown to testers, e.g. '3D weapons (0.8)')."
}

# --- Load App Secrets from store.properties ----------------------------------
$storeProps = Join-Path $android "store.properties"
if (-not (Test-Path $storeProps)) {
    throw "Missing $storeProps. Copy store.properties.template to store.properties and fill in the App Secrets."
}
$secrets = @{}
foreach ($line in Get-Content $storeProps) {
    $t = $line.Trim()
    if ($t -eq "" -or $t.StartsWith("#")) { continue }
    $kv = $t -split "=", 2
    if ($kv.Count -eq 2) { $secrets[$kv[0].Trim()] = $kv[1].Trim() }
}

# --- Optional rebuild of the requested flavors -------------------------------
if (-not $SkipBuild) {
    $env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
    $tasks = $Apps | ForEach-Object { ":app:assemble$($catalog[$_].Flavor)Release" }
    Write-Host "Building: $($tasks -join ' ')" -ForegroundColor Cyan
    Push-Location $android
    try {
        & .\gradlew.bat @tasks
        if ($LASTEXITCODE -ne 0) { throw "Gradle build failed (exit $LASTEXITCODE)." }
    } finally { Pop-Location }
}

# --- Locate ovr-platform-util ------------------------------------------------
if (-not (Test-Path $OvrUtil)) {
    throw "ovr-platform-util not found at '$OvrUtil'. Pass -OvrUtil <path> or install it."
}

# --- Preflight: every requested app must have a secret + a built APK ---------
$fail = $false
foreach ($a in $Apps) {
    $key = "secret.$a"
    if (-not $secrets.ContainsKey($key) -or $secrets[$key] -eq "" -or $secrets[$key] -eq "CHANGE_ME") {
        Write-Host "  [!] $a : missing/placeholder $key in store.properties" -ForegroundColor Red; $fail = $true
    }
    if (-not (Test-Path $catalog[$a].Apk)) {
        Write-Host "  [!] $a : APK not found ($($catalog[$a].Apk)) — build it first (drop -SkipBuild)" -ForegroundColor Red; $fail = $true
    }
}
if ($fail) { throw "Preflight failed — fix the items above and re-run." }

# --- Upload each app ---------------------------------------------------------
$results = @()
foreach ($a in $Apps) {
    $app = $catalog[$a]
    $secret = $secrets["secret.$a"]

    # Surface the versionCode we're actually shipping (helps catch a forgotten bump).
    $vc = "?"
    $aapt2 = Get-ChildItem "$env:LOCALAPPDATA\Android\Sdk\build-tools\*\aapt2.exe" -ErrorAction SilentlyContinue |
             Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    if ($aapt2) {
        $b = & $aapt2 dump badging $app.Apk 2>$null | Select-String "versionCode='(\d+)'"
        if ($b) { $vc = $b.Matches[0].Groups[1].Value }
    }

    Write-Host ""
    Write-Host "==> $a  (app-id $($app.AppId), versionCode $vc) -> channel $Channel" -ForegroundColor Green
    $utilArgs = @(
        "upload-quest-build",
        "--app-id", $app.AppId,
        "--app-secret", $secret,
        "--apk", $app.Apk,
        "--channel", $Channel,
        "--age-group", $AgeGroup,
        "--notes", $Notes
    )
    if ($DryRun) {
        $shown = ($utilArgs -join " ") -replace [regex]::Escape($secret), "***"
        Write-Host "    [dry-run] $OvrUtil $shown" -ForegroundColor DarkGray
        $results += [pscustomobject]@{ App = $a; versionCode = $vc; Result = "dry-run" }
        continue
    }

    & $OvrUtil @utilArgs
    if ($LASTEXITCODE -ne 0) {
        $results += [pscustomobject]@{ App = $a; versionCode = $vc; Result = "FAILED (exit $LASTEXITCODE)" }
        Write-Host "    upload FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
    } else {
        $results += [pscustomobject]@{ App = $a; versionCode = $vc; Result = "uploaded" }
    }
}

Write-Host ""
Write-Host "Summary ($Channel):" -ForegroundColor Cyan
$results | Format-Table -AutoSize
if ($results | Where-Object { $_.Result -like "FAILED*" }) { exit 1 }
