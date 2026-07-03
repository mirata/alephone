# VR 3D-model GPU keyframe-lerp path (GZDoom-style)

Goal: eliminate the per-frame CPU cost of rendering complex 3D weapon models on Quest by
keeping keyframes GPU-resident in persistent VBOs and doing interframe blending in a vertex
shader — exactly what QuestZDoom/GZDoom does. This restores smooth interframe blending *and*
fixes the frame-rate cliff at the same time (blending becomes free instead of a tradeoff).

## Why the current path is slow (measured from code)

Every model draw on Android does the whole mesh **three times, per eye, per frame**:

1. `Model3D::FindPositions_MD3Frame` (`Model3D.cpp:1145`) reposes the shared `Positions`
   array on the CPU (memcpy, or lerp when blending).
2. `ModelRenderer::Render` submits it via **client-side vertex arrays**
   (`ModelRenderer.cpp:42`, `glVertexPointer(... Model.PosBase())`).
3. The GLES shim's draw-flush (`gl_es_compat.cpp:581-592`, `flushEngine`) walks the client
   arrays into an interleaved `buf` and re-uploads it every draw with
   `glBufferData(GL_ARRAY_BUFFER, ..., GL_STREAM_DRAW)`.

Worst case (blended skin + glow map) the model is drawn **one triangle per `glDrawElements`**
(`ModelRenderer.cpp:119-131`), i.e. thousands of draw calls/frame/eye.

## Data already in the right shape (`Model3D.h`)

- `MD3Positions` — `MD3NumFrames × NumVerts × 3` floats, laid out per keyframe (line 187).
- `MD3Normals`   — same shape.
- `TxtrCoords`   — `NumVerts × 2`, shared across all frames.
- `VertIndices`  — triangle indices, shared across all frames.

This is exactly GZDoom's layout, so persistent static buffers are a direct upload.

## Design

### 1. Persistent GPU buffers (upload once at model load)
Add to `OGL_ModelData` (Android-only; handles stay 0 elsewhere):
- `GLuint vboPos`   — all frames of `MD3Positions`  (`GL_STATIC_DRAW`)
- `GLuint vboNorm`  — all frames of `MD3Normals`    (`GL_STATIC_DRAW`)
- `GLuint vboTex`   — `TxtrCoords`                  (`GL_STATIC_DRAW`)
- `GLuint ibo`      — `VertIndices`                 (`GL_STATIC_DRAW`)
- `GLuint vao`, plus `int numVerts`, `int numIndices`.

Upload in `OGL_ModelData::Load` after the model + `MD3Positions` are built. Note
`FindPositions_Neutral`/the load-time transform bakes the model rotation/scale/shift into every
keyframe already (`OGL_Model_Def.cpp:588-604`), so the uploaded keyframes are pre-transformed —
no per-draw model matrix needed beyond the controller placement matrix.

Guard so this only runs for MD3 models (`MD3NumFrames > 0`); static single-frame models still get
one persistent buffer (frameA == frameB, mix = 0).

Free the buffers in `Unload`.

### 2. New vertex shader `model_lerp.vert`
Attributes:
- `layout(location=0) in vec3 posA;`
- `layout(location=4) in vec3 posB;`   (reuse the normal slot's index space; pick free locations)
- `in vec3 normA; in vec3 normB;`
- `in vec2 texcoord;`

Uniforms (same set `flushEngine` feeds, so we can reuse `engineLocsFor`):
- `u_mvp`, `u_mv`, `u_normalMatrix`, `u_texMat0`, fog uniforms, `u_alphaTest/u_alphaRef`,
  `u_brightness`, `u_depthScale`, and **`u_mix`** (new).

Body:
```glsl
vec3 pos  = mix(posA,  posB,  u_mix);
vec3 nrm  = mix(normA, normB, u_mix);
gl_Position = u_mvp * vec4(pos, 1.0);
// ... same varyings sprite.vert produces (texcoord, fog depth, classicDepth, etc.)
```
Pair it with the existing fragment shaders to make three programs:
`model_lerp.vert × {sprite.frag, invincible.frag, sprite_infravision.frag}`
covering normal / static-invincibility / infravision (matches `OGL_Render.cpp:3418-3427`).

### 3. Shim accessor for current matrices
`flushEngine` computes `mvp = projection.top() * modelview.top()` from the CPU matrix stack
(`gl_es_compat.cpp:604-605`). The bypass draw needs the same value. Add:
```c
void a1ffGetMVP(float* out16);   // projection.top() * modelview.top()
void a1ffGetMV(float* out16);
```
so the weapon draw (which has already pushed its controller matrix via `glMultMatrixf`,
`OGL_Render.cpp:3416`) can fetch MVP after the push and set `u_mvp` directly.

### 4. New draw function `OGL_DrawVRWeaponModelGPU`
Replaces the `ModelRenderObject.Render(...)` calls inside `OGL_RenderVRWeaponModel`
(`OGL_Render.cpp:3418-3427`) when the model has persistent buffers:
1. Bind the skin (reuse `NormalShader`/glow/infravision texture-bind logic).
2. `glUseProgram(modelLerpProgram[variant])`.
3. Fetch MVP via `a1ffGetMVP`, set `u_mvp` + `u_mix` + fog/alpha/brightness uniforms.
4. `glBindVertexArray(vao)`; bind `vboPos` to loc0 at offset `frameA*numVerts*3*4` and loc(posB)
   at offset `frameB*numVerts*3*4`; same for normals; bind `vboTex` to texcoord; bind `ibo`.
5. `glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, 0)` — **one call, no upload**.
Keep the old `ModelRenderObject.Render` path as a fallback when buffers are absent (desktop, or
non-MD3), so nothing regresses.

## Scope for the tracer bullet
VR weapon model only (`OGL_RenderVRWeaponModel`). World sprite-replacement models keep the old
path until the weapon path is confirmed on-device. Widening to all VR models later reuses the
same buffers + shaders but must also handle the depth-sort/blended multi-pass logic in
`ModelRenderer::Render`.

## As-built (differs from the sketch above)
Implemented 2026-07-03 (branch `questvr`). The design simplified during implementation:

- **Dedicated shim program, not the engine preamble.** Instead of adding morph attributes to the
  shared GLSL preamble + reusing sprite/invincible/infravision programs, there is a small
  self-contained program in the shim (`g_morphProg` in `gl_es_compat.cpp`, sibling of the existing
  builtin program): loc 0 = frame-A position, loc 5 = frame-B position, loc 1 = texcoord,
  `uMix` lerps, `uColor` tints the sampled skin. This avoids any dependency on which engine
  program happens to be bound at weapon-draw time (the normal weapon path leaves that ambiguous).
- **Entry point** `a1ffDrawMorphMesh(posVBO, texVBO, ibo, numVerts, numIndices, frameA, frameB,
  mix, color4)` (`gl_es_compat.{h,cpp}`): takes MVP from the shim's matrix stack (so the caller's
  pushed controller matrix is honored), binds the persistent VBOs, one `glDrawElements`, then
  restores shared-VAO state (disables loc 5, unbinds the element buffer).
- **Persistent buffers** live on `OGL_ModelData` (`VR_PosVBO`/`VR_TexVBO`/`VR_IBO` +
  `VR_NumVerts`/`VR_NumIndices`), uploaded in `VR_UploadBuffers()` at the end of `Load()`, freed in
  `Unload()`. Guarded `#if defined(__ANDROID__)`; desktop untouched.
- **Scope: normal weapon case only.** Static-invincibility and infravision weapons (powerup-only,
  rare) keep the CPU `ModelRenderer::Render` path, so their effect shaders need no morph variant.
  The normal branch calls `NormalShader(nullptr)` first to bind the skin + blend exactly like the
  CPU path, then `a1ffDrawMorphMesh`.
- Falls back to the CPU path when `VR_PosVBO == 0` (non-MD3 models).

Status: compiles clean, deployed to device — **awaiting on-device visual + framerate confirmation.**
Watch for: weapon renders with correct skin/orientation/placement, interframe animation still
smooth, and the framerate cliff on the complex model gone.

## Open risks / verify on-device
- Attribute location collisions with the shim's fixed 0/1/2/3/4 layout — the bypass binds its own
  VAO so it is independent, but double-check `glBindVertexArray(0)`/state restore before returning
  to the shim's draws.
- Blended skins previously disabled depth writes (`OGL_Render.cpp:2547`); preserve that.
- Winding/`glFrontFace` mirroring for off-hand dual-wield (`OGL_Render.cpp:3383-3395`) must still
  apply — it's GL state, independent of the draw path, so it should carry over unchanged.
- `u_mix` must be 0 for static/single-frame models to avoid reading an out-of-range frame B.
