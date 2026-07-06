package org.alephone;

import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.system.Os;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Aleph One entry activity for Phase 0 (flat / non-VR).
 *
 * SDLActivity loads the native libraries returned by {@link #getLibraries()} and then calls
 * {@code SDL_main} (which is Aleph One's {@code main()} in Source_Files/main.cpp, renamed by
 * SDL2/SDL_main.h on Android). No engine entry-point change is required.
 *
 * Phase 2 (VR) will replace this with an OpenXR-hosted activity where TBXR owns the EGL context;
 * see docs/VR_PORT_PLAN.md.
 */
public class AlephOneActivity extends SDLActivity {

    private static final String TAG = "A1VR";

    /**
     * Branded, store-installable builds (the marathon / marathon2 / infinity flavors) ship their
     * base data + scenario INSIDE the APK under assets/game (assembled at build time — see
     * app/build.gradle). Extract it once to the app's private files dir and point the engine at it
     * via ALEPHONE_DEFAULT_DATA, which shell.cpp already honors as the default scenario dir. The
     * `dev` flavor bundles nothing, so this is a no-op there and the engine falls back to its normal
     * external-storage data dir (what deploy.ps1 pushes over adb). Runs BEFORE super.onCreate so the
     * env var is set before SDLActivity starts the native SDL_main thread.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            extractBundledData();
        } catch (Exception e) {
            Log.e(TAG, "Bundled data extraction failed", e);
        }
        super.onCreate(savedInstanceState);
    }

    private void extractBundledData() throws Exception {
        // No assets/game -> this is the `dev` flavor; nothing to bundle, behave as before.
        String[] top = getAssets().list("game");
        if (top == null || top.length == 0) {
            return;
        }

        File dest = new File(getFilesDir(), "game");
        File marker = new File(dest, ".bundle_version");
        String version = currentVersion();

        if (!marker.exists() || !version.equals(readFile(marker))) {
            Log.i(TAG, "Extracting bundled game data to " + dest.getAbsolutePath());
            deleteRecursive(dest);
            dest.mkdirs();
            copyAsset("game", dest);
            writeFile(marker, version);
            Log.i(TAG, "Bundled game data ready");
        }

        Os.setenv("ALEPHONE_DEFAULT_DATA", dest.getAbsolutePath(), true);
    }

    private void copyAsset(String assetPath, File dest) throws Exception {
        String[] children = getAssets().list(assetPath);
        if (children != null && children.length > 0) {
            // Directory: recurse.
            dest.mkdirs();
            for (String child : children) {
                copyAsset(assetPath + "/" + child, new File(dest, child));
            }
        } else {
            // File: copy bytes.
            try (InputStream in = getAssets().open(assetPath);
                 OutputStream out = new FileOutputStream(dest)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                while ((n = in.read(buf)) != -1) {
                    out.write(buf, 0, n);
                }
            }
        }
    }

    private String currentVersion() {
        try {
            PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
            return pi.versionName + "/" + pi.versionCode;
        } catch (Exception e) {
            return "unknown";
        }
    }

    private static String readFile(File f) {
        try (InputStream in = new java.io.FileInputStream(f)) {
            byte[] b = new byte[(int) f.length()];
            int off = 0, n;
            while (off < b.length && (n = in.read(b, off, b.length - off)) != -1) off += n;
            return new String(b, 0, off, "UTF-8").trim();
        } catch (Exception e) {
            return "";
        }
    }

    private static void writeFile(File f, String s) throws Exception {
        try (OutputStream out = new FileOutputStream(f)) {
            out.write(s.getBytes("UTF-8"));
        }
    }

    private static void deleteRecursive(File f) {
        if (f == null || !f.exists()) return;
        File[] kids = f.listFiles();
        if (kids != null) {
            for (File k : kids) deleteRecursive(k);
        }
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }

    @Override
    protected String[] getLibraries() {
        // vcpkg's arm64-android triplet builds everything STATIC, so SDL2 (and
        // SDL2_image/ttf, OpenAL, etc.) are linked into libmain.so — there is no
        // separate libSDL2.so to load. SDL's JNI is wired via JNI_OnLoad, which the
        // CMake build force-keeps (-u JNI_OnLoad) so RegisterNatives runs on load.
        return new String[] { "main" };
    }

    @Override
    protected String[] getArguments() {
        // Command-line args passed to Aleph One's main(). Point at on-device data here later.
        return new String[] {};
    }

    /**
     * Immersive VR (Phase 2): the OpenXR compositor — not the 2D SurfaceView — owns the display, so
     * the activity window never holds stable focus. SDLActivity gates starting SDL_main (and
     * pauses) on {@code mHasFocus}, so without this the engine thread never launches. Force
     * "focused" so SDL starts and keeps running while OpenXR drives presentation.
     */
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(true);
    }
}
