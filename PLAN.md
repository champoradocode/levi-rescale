# PLAN — Levi‑ReScale (Method B: graphics-API render scaling)

Target: Minecraft Bedrock **1.26.52** on LeviLaunchroid, native mod (`arm64-v8a`).
Goal: scale the internal render resolution to save battery / reduce heat, without
touching Android display resolution, aspect ratio, or touch mapping.

Reference analysis lives in [§0](#0-analysis-reference-doc-vs-our-reality).
Future (post-v1) ideas live in [§9](#9-future-plans-not-in-v1).

---

## How to work through this

- Tasks are ordered and **sequential**. One feature at a time, one task at a time.
- Every task should be **≤ ~30 min of focused work**. If a task overruns, split it
  and tell me so the plan gets updated.
- Each task has a **Done when** gate. Do not start the next task until the gate
  passes. A failing gate is information, not an obstacle — report it.
- Tick the box (`- [ ]` → `- [x]`) when a task is done.
- Commit per task. Suggested message: `T14: scale glViewport with guards`.
- Logcat used throughout:
  ```bash
  adb logcat -c
  adb logcat -v color -s Levi:* Levi-ReScale:* Preloader:* ModNativeLoader:*
  ```

## Task index

| Feature | Tasks | State |
|---|---|---|
| F0 — Scaffolding & rename | [T01–T03](#f0--scaffolding--rename-t01t03) | not started |
| F1 — Configuration | [T04–T08](#f1--configuration-t04t08) | not started |
| F2 — Hook resolution & lifecycle | [T09–T13](#f2--hook-resolution--lifecycle-t09t13) | in progress |
| F3 — Viewport scaling (core) | [T14–T18](#f3--viewport-scaling-core-t14t18) | not started |
| F4 — Window buffer geometry upscale | [T19–T24](#f4--window-buffer-geometry-upscale-modewindow-t19t24) | not started |
| F5 — FBO upscale fallback | [T25–T30](#f5--fbo-upscale-fallback-modefbo-t25t30) | not started |
| F6 — Device validation | [T31–T34](#f6--device-validation-t31t34) | not started |
| F7 — Docs & packaging | [T35–T37](#f7--docs--packaging-t35t37) | not started |

Critical path: F0 → F1 → F2 → F3 → F4 → F5 → F6 → F7.
F4 and F5 are alternatives; F4 must be attempted and rejected before F5 starts.

---

## 0. Analysis: reference doc vs. our reality

Source doc: `minecraft_launcher_resolution_scaling.md` (written about **Java**
launchers such as Pojav/Mojo).

1. **Method A is unavailable to us.** It needs Java-side `LayoutParams` changes in
   the launcher app. A native `.so` mod cannot do that. Method B is not just
   "nicer" — it is the only route available from a LeviLauncher native mod.
2. **The doc's Method B is incomplete.** Shrinking the viewport alone renders into
   a *corner sub-rect* of the native swapchain. The GPU will **not** upscale it for
   us, so an upscale step is mandatory (platform compositor via a smaller window
   buffer, or our own blit).
3. **The doc overpromises "crisp HUD".** In Bedrock the world and the HUD share the
   same default framebuffer, so scaling the viewport scales the HUD too. Selective
   world-only scaling is a research project — future plan F1, not v1.

Locked decisions for v1:

| Decision | Choice |
|---|---|
| Upscaling strategy | Window buffer geometry **first**, offscreen FBO as config-gated fallback (`mode: auto\|window\|fbo`) |
| Graphics backend | GLES only in v1 (Vulkan → future plan F2) |
| HUD crispness | Uniform scaling only in v1 (`scaleUi` reserved, ignored — future plan F1) |
| Scale range | 0.5 – 1.0 (schema exposes min/max only, no `step`) |
| Resulting visual | Same as Method A (everything at `S`), but with **zero layout risk** — the game's own layout math still runs at native size |

---

## F0 — Scaffolding & rename (T01–T03)

- [x] **T01 — Rename the mod class and namespace.**
  `src/mod/MyMod.{h,cpp}` → `src/mod/RescaleMod.{h,cpp}`, `ClangeMeMod` →
  `RescaleMod`, namespace `clange_me` → `levi_rescale`, update `src/main.cpp`.
  *Done when:* the `.levipack` builds via CI and the mod still loads and logs
  `Loaded Levi-ReScale from …`.

- [x] **T02 — Create the hook source stubs and wire them into CMake.**
  Empty `src/hooks/GlesResolver.{h,cpp}`, `ViewportScale.{h,cpp}`,
  `WindowGeometry.{h,cpp}`, `FboComposite.{h,cpp}`; add all to the
  `${MOD_LIBRARY_NAME}` target in `CMakeLists.txt` (and to `levi_config_generator`
  sources where needed).
  *Done when:* host config generator + Android build both succeed, no warnings.

- [x] **T03 — Clean up leftover template text.**
  Remove `message = "Hello from clange_me"` style template fields and the
  `clange_me` naming from `README.md`, `manifest.json.in` (if any) and config
  defaults, so logs and config UI read `levi_rescale`.
  *Done when:* no `clange_me` / `Clange Me` string remains in `src/`,
  `README.md`, or generated `config.json`.

---

## F1 — Configuration (T04–T08)

Final config surface (also documented in [§2](#2-config-surface-final)):

| Field | Type | Default | Schema notes |
|---|---|---|---|
| `version` | `int` | `1` | `readOnly` |
| `enabled` | `bool` | `true` | — |
| `scale` | `float` | `1.0` | `minimum = 0.5`, `maximum = 1.0` |
| `mode` | `enum RescaleMode { Auto, Window, Fbo }` | `Auto` | enum schema generated for free via `magic_enum` |
| `scaleUi` | `bool` | `false` | reserved for future plan F1; v1 ignores it |
| `debug` | `bool` | `false` | verbose hook + per-frame logging |

`pl::config::FieldSchema` supports only `title`, `description`, `minimum`,
`maximum`, `readOnly` — **no `step`**, so the launcher UI rounds. Enums serialize by
name (`magic_enum::enum_name`), keeping `config.json` human-editable.

- [ ] **T04 — Add the enum and the new config fields.**
  `enum class RescaleMode { Auto, Window, Fbo };` and extend `ModConfig` with
  `scale`, `mode`, `scaleUi`, `debug`. Drop the template's `message` field.
  *Done when:* `Config.h` compiles and `ModConfig` remains an aggregate with an
  integral `version` (satisfies the `TypedConfig` concept).

- [ ] **T05 — Write the schema entries.**
  One `pl::config::Schema<ModConfig>::field()` branch per field with `title`,
  `description`, and `minimum`/`maximum` on `scale`.
  *Done when:* generated `config.schema.json` shows `minimum: 0.5`, `maximum: 1.0`
  on `scale` and `"enum": ["Auto","Window","Fbo"]` on `mode`.

- [ ] **T06 — Generate and eyeball the default config.**
  Run the host `levi_generate_config` target; inspect `config.json` and
  `config.schema.json`.
  *Done when:* `config.json` contains all six keys with the intended defaults and
  is human-readable.

- [ ] **T07 — Sanitize values after load.**
  Clamp `scale` into `[0.5, 1.0]`; snap to 2 decimals; map an unknown `mode` to
  `Auto`. Put this in one helper (`sanitize(ModConfig&)`), called from `load()`.
  *Done when:* hand-editing `config.json` to `scale: 0.1` or `"mode": "banana"`
  loads without error and yields `0.5` / `Auto`.

- [ ] **T08 — Persist sanitized values.**
  If sanitization changed anything, `mConfigFile->save(mConfig)`; log at `info`
  when it rewrites.
  *Done when:* a bad value is corrected in `config.json` on the next launch and
  the log says it was corrected.

---

## F2 — Hook resolution & lifecycle (T09–T13)

- [ ] **T09 — Introduce a hook-state container.**
  A `HookSet` in `RescaleMod` holding `std::optional<pl::memory::HookHandle>`s (or
  target+detour pairs for `unhook`), with `installAll()` / `removeAll()`.
  *Done when:* compiles, and `removeAll()` is idempotent and safe to call twice.

- [x] **T10 — Resolve the GL viewport functions.**
  `resolveSignatures` for `glViewport` and `glScissor` over the candidate module
  list, preferring `libGLESv2.so`, then `libGLESv3.so`, then `libGLESv1_CM.so`;
  log each resolved address + module name; treat `0` as failure.
  *Done when:* logcat shows both addresses on a cold start with no GL installed.

- [x] **T11 — Resolve the EGL functions.**
  Same for `eglCreateWindowSurface` and `eglSwapBuffers` in `libEGL.so`.
  *Done when:* both addresses logged; missing ones log `warn` and are skipped
  without aborting load.

- [ ] **T12 — Deferred install when GL is not loaded yet.**
  `enable()` runs **before** `super.onCreate()`. If resolution fails, hook
  `android_dlopen_ext` (libdl) and retry resolution once from inside that detour.
  *Done when:* hooks install on a cold start regardless of GL library load order;
  the dlopen detour removes itself after a successful install.

- [ ] **T13 — Wire install/teardown into the lifecycle.**
  `enable()` → install; `disable()` → `removeAll()`; `unload()` → drop config
  file. Install at the current `scale = 1.0`, i.e. detours must be **no-ops**
  until F3.
  *Done when:* the game renders identically with the mod enabled; three
  pause/resume cycles produce no GL errors and no leak warnings in logcat.

---

## F3 — Viewport scaling, the core (T14–T18)

Deliberately still visually broken at the end of this feature (image in a corner
sub-rect). That is expected and is the proof the intercept works; F4/F5 fix it.

- [ ] **T14 — Write the pure scale transform.**
  A small free function: given `(x, y, w, h)` and `S`, return the scaled rect —
  `w' = max(1, round(w*S))`, `h' = max(1, round(h*S))`, `x`/`y` untouched, plus
  the guard predicates (min-size, surface-size match). Keep it free of GL calls so
  it is trivially reviewable and testable.
  *Done when:* the function has no GL/`gfx` dependency and its behaviour for
  `S=1.0`, odd sizes and sub-threshold sizes is documented in a comment-free
  header-level doc block or the task notes.

- [ ] **T15 — Hook `glViewport`.**
  Detour: read `S` from an atomic, evaluate guards, call `original` with the
  transformed rect. No allocation, no locking, no logging in the hot path.
  *Done when:* at `S = 0.75` the rendered image occupies ~56 % of the screen area
  in the expected corner, and `S = 1.0` is bit-identical to no mod.

- [ ] **T16 — Hook `glScissor` with the same transform.**
  Clipping must be scaled identically to the viewport or GUI/scissor rects break.
  *Done when:* no misplaced clipping artifacts; guards behave the same as T15.

- [ ] **T17 — Make `scale` live-appliable.**
  Publish `S` to a `std::atomic<float>`; no re-hooking when it changes.
  *Done when:* editing `scale` and relaunching changes the render size with no
  hook reinstall in the log.

- [ ] **T18 — Add the debug logging path.**
  Under `debug`, log the first N viewport transforms per frame (frame counter, not
  per-call spam) with old → new rects.
  *Done when:* `debug = true` produces a readable trace of one frame; default
  `debug = false` produces no extra log lines.

---

## F4 — Window buffer geometry upscale, `mode=window` (T19–T24)

The platform compositor performs the upscale; no FBO, no blit code.

- [ ] **T19 — Capture the `ANativeWindow*`.**
  Detour `eglCreateWindowSurface`, stash the `native_window` argument (and the
  requested width/height) in mod state. Do not alter the call.
  *Done when:* logcat shows the captured pointer and the game's requested size.

- [ ] **T20 — Request a scaled buffer.**
  Resolve `ANativeWindow_setBuffersGeometry` (libandroid) and call it with
  `round(W*S) x round(H*S)` and the original format. Log the return value.
  *Done when:* the scaled image **fills the whole screen** at `S = 0.75` and
  `0.5`, letterbox bars unchanged, aspect ratio unchanged.

- [ ] **T21 — Restore native geometry on teardown.**
  `disable()` calls `ANativeWindow_setBuffersGeometry(win, 0, 0, fmt)` (0,0 =
  native) before removing hooks.
  *Done when:* after pause/resume the game renders full-screen with the mod
  disabled and no stale offset.

- [ ] **T22 — Handle surface recreation.**
  The window pointer and size change on resume/rotation; update state on each
  `eglCreateWindowSurface` and re-apply geometry, without leaking the old pointer
  or double-applying.
  *Done when:* rotate and pause/resume repeatedly; render size stays correct and
  matches the current surface.

- [ ] **T23 — Rate-limit and detect a resize loop.**
  Ignore geometry changes closer together than a small interval; log a `warn` when
  the engine fights us, and mark `mode=window` as failed so `auto` can fall back.
  *Done when:* a forced geometry fight produces one warning, not a log flood, and
  the game stays responsive.

- [ ] **T24 — Close out `mode=window`.**
  Confirm `mode = window` works and `mode = auto` selects it.
  *Done when:* `mode=auto` logs `selected window-geometry upscale`; `mode=fbo`
  logs that FBO is not implemented yet (until F5 lands).

---

## F5 — FBO upscale fallback, `mode=fbo` (T25–T30)

Only start once F4 is done and its outcome is known.

- [ ] **T25 — Own FBO + texture at scaled size.**
  Create (and lazily resize) an FBO with an RGBA8 texture at
  `round(W*S) x round(H*S)`. All GL work on the GL thread only; create on first use,
  destroy in `disable()`.
  *Done when:* FBO creation logs dimensions; no GL errors on resize.

- [ ] **T26 — Redirect rendering into the FBO.**
  Detour `eglSwapBuffers`: after the real present, bind our FBO so the next frame
  renders into it.
  *Done when:* the game renders into the offscreen target; presenting the screen
  directly shows the last composited frame.

- [ ] **T27 — Composite and present.**
  Before the real `eglSwapBuffers`, unbind to the default framebuffer and draw a
  fullscreen textured quad with `GL_LINEAR`/`GL_CLAMP_TO_EDGE`. Guard with a
  `thread_local` re-entrancy flag around our own GL calls.
  *Done when:* `mode=fbo` at 0.75/0.5 fills the screen correctly and matches the
  window-geometry look.

- [ ] **T28 — Save and restore GL state.**
  Restore framebuffer, viewport, scissor, blend/depth/stencil, active texture,
  program, VAO — whatever the blit touched. No persistent state changes.
  *Done when:* the game's own rendering after the composite is unaffected (no
  missing HUD, no depth-test artifacts).

- [ ] **T29 — Implement `mode=auto` selection.**
  Prefer window geometry; fall back to FBO when T23's failure signal fires or when
  `setBuffersGeometry` is unsupported. Log the decision once.
  *Done when:* `auto` picks a working path on this device and the choice is
  visible in one log line.

- [ ] **T30 — Close out the upscale feature.**
  Both `window` and `fbo` paths verified at 0.75 and 0.5.
  *Done when:* `mode` can be flipped between all three values across restarts with
  correct visuals and no GL errors in logcat.

---

## F6 — Device validation (T31–T34)

- [ ] **T31 — Baseline at `scale = 1.0`.**
  Frame time (in-game counter), `dumpsys batterystats`, thermal zone readings, over
  a fixed 5-minute world session. Record the numbers in the task notes.
  *Done when:* a baseline table exists to compare against.

- [ ] **T32 — Scaled measurements.**
  Repeat T31 at `0.75` and `0.5`, for both upscale modes.
  *Done when:* the battery/thermal delta per scale is documented, and the best
  setting for this device is identified.

- [ ] **T33 — Regression pass.**
  Rotation, main menu, inventory, chat, world load/reload, pause/resume ×3,
  switching `mode` and `scale` between launches.
  *Done when:* every item renders correctly; failures are recorded as new tasks
  appended to the relevant feature.

- [ ] **T34 — Log hygiene.**
  Confirm default config produces no per-frame or per-call logging; `debug` output
  is opt-in only.
  *Done when:* a 60-second session at defaults adds zero log lines from the mod
  beyond load/enable/disable.

---

## F7 — Docs & packaging (T35–T37)

- [ ] **T35 — README rewrite.**
  What it does, how it works (2 paragraphs, no inner workings), install steps,
  full config reference with defaults and what each `mode` does, measured results
  from T32, and a link to `PLAN.md`.
  *Done when:* a reader can install and configure the mod from the README alone.

- [ ] **T36 — Reconcile the reference doc.**
  Annotate `minecraft_launcher_resolution_scaling.md` as Java-launcher-specific
  and point to the analysis section of `PLAN.md`, so the two do not contradict.
  *Done when:* the doc's Method A/B framing no longer implies an unimplemented
  Java-side path for this mod.

- [ ] **T37 — Release build.**
  Bump `MOD_VERSION`, run the CI build, produce
  `levi_rescale-<version>-arm64-v8a.levipack`, and sideload-import it once as a
  clean-install smoke test.
  *Done when:* a fresh install (no manual file copying) loads and applies the
  configured scale.

---

## 2. Config surface (final)

| Field | Type | Default | Schema notes |
|---|---|---|---|
| `version` | `int` | `1` | `readOnly` |
| `enabled` | `bool` | `true` | — |
| `scale` | `float` | `1.0` | `minimum = 0.5`, `maximum = 1.0` |
| `mode` | `enum RescaleMode { Auto, Window, Fbo }` | `Auto` | enum schema generated for free via `magic_enum` |
| `scaleUi` | `bool` | `false` | reserved for future plan F1; v1 ignores it |
| `debug` | `bool` | `false` | verbose hook + per-frame logging |

`pl::config::FieldSchema` supports only `title`, `description`, `minimum`,
`maximum`, `readOnly` — **no `step`**, so the launcher UI rounds. Enums serialize by
name (`magic_enum::enum_name`), keeping `config.json` human-editable.

Config path: `<modRoot>/config/config.json`, written atomically (temp + rename) by
`pl::config::ConfigFile`.

---

## 3. File layout (target)

```
src/main.cpp                      PL_REGISTER_MOD(levi_rescale::RescaleMod, …)
src/mod/RescaleMod.{h,cpp}        lifecycle + scale state + hook ownership
src/mod/Config.{h,cpp}            ModConfig, RescaleMode, Schema, sanitize()
src/hooks/GlesResolver.{h,cpp}    resolve GL/EGL entry points across candidate modules
src/hooks/ViewportScale.{h,cpp}   glViewport + glScissor detours, pure scale transform, guards
src/hooks/WindowGeometry.{h,cpp}  ANativeWindow capture + setBuffersGeometry (mode=window)
src/hooks/FboComposite.{h,cpp}    offscreen FBO + linear upscale blit (mode=fbo)
CMakeLists.txt                    sources on ${MOD_LIBRARY_NAME}; host generator too
README.md                         user docs
PLAN.md                           this file
```

---

## 4. Guardrails (non-negotiable, apply to every task)

- Never scale only one axis — aspect ratio must stay identical.
- Never scale below the min-size threshold (256 px) in either dimension.
- Never double-scale an already-scaled viewport.
- No allocation, locking, or logging inside GL detours.
- Check every pointer and every `hook()` return value; fail gracefully with a
  `warn` and skip that hook.
- Restore all global state in `disable()` so pause/resume is transparent.
- Unique patch/hook names, no collisions with other mods.

---

## 9. Future plans (not in v1)

These are explicitly **out of scope** for v1 and are not tracked as tasks above.
Each becomes its own plan file when we get to it.

### F1 — World-only scaling (crisp HUD)
The doc's Method B selling point. In Bedrock the world and HUD share the default
framebuffer, so a viewport scale blurs the HUD too.
- Research questions: is there a per-pass signal (render stage, HUD batch, framebuffer
  binding) that distinguishes the world pass from GUI passes?
- Candidate approaches: hook the world render target creation; composite the HUD in a
  second pass at native resolution; use a scissor/viewport state fingerprint.
- Config already reserved: `scaleUi`.
- Risk: may be infeasible without engine-level reimplementation.

### F2 — Vulkan backend
Some devices/launcher configurations run Bedrock on Vulkan.
- `vkCmdSetViewport` / `vkCmdSetScissor` detours (same transform and guards).
- Swapchain `VkExtent2D` scaling at `vkCreateSwapchainKHR` (+ recreation handling).
- Function pointers via `vkGetInstanceProcAddr`/`vkGetDeviceProcAddr`, resolved
  lazily once the device exists.
- Backend detection and a `mode`-independent config surface.

### F3 — Live slider reload
Watch `config.json` (inotify or mtime poll) and re-apply scale without relaunching.

### F4 — Extended scale range
Allow `< 0.5` for extreme battery saver, with a floor so the GUI does not break.

### F5 — Per-preset profiles
Named presets (e.g. `battery`, `balanced`, `quality`) selectable from the config UI.

---

## 6. Out of scope for v1

- Vulkan (→ future plan F2).
- World-only scaling / crisp HUD (→ future plan F1).
- Live config reload (→ future plan F3).
- Scale range below 0.5 (→ future plan F4).
- Per-preset profiles (→ future plan F5).
