# LeviLauncher Native Template Mod (LeviLaunchroid + preloader-android)

This is a **mod template** for creating native (C++) mods for LeviLauncher on Android. It uses [preloader-android](https://github.com/LiteLDev/preloader-android) as the runtime SDK and targets LeviLaunchroid's native mod loading system.

## 1. Goal & Scope

- **Purpose**: Template to write native mods for **LeviLauncher (Android)**. This is **not** the preloader itself.
- **Target ABI**: Primarily `arm64-v8a`. `armeabi-v7a` is also supported by the build system but the main target is ARM64. Android 11+.
- **Minecraft version**: **Strictly 1.26.52**. Set `minecraft_versions` to `["1.26.52"]` for your mod.
- **Architecture**: Native C++20 mod loaded as a `.so` by LeviLauncher via preloader-android. No public MCPE API — you must hook via preloader APIs (signatures/patterns + vtable resolution).

## 2. Workspace Architecture

- **SDK**: `preloader-android` v0.2.2 is fetched via CMake FetchContent (`cmake/preloader-sdk` subdir). Public headers are provided under `include/pl/` at build time (exposed via `LEVI_PRELOADER_INCLUDE_DIR`).
- **Entry**: `src/main.cpp` uses `PL_REGISTER_MOD(...)` to register lifecycle with preloader.
- **Mod logic**: `src/mod/MyMod.{h,cpp}` — lifecycle (`load/enable/disable/unload`), logger, config dirs, typed config.
- **Config**: Typed config with schema via `pl::config` + `pl::config::Schema<T>`. Config JSON/schema generation supported (see §3).
- **Packaging**: Produces `.levipack` (ZIP) consumable by LeviLauncher: contains `package/<MOD_ID>/lib<MOD_LIBRARY_NAME>.so`, `manifest.json`, `config/`, optional icon.

Relevant template files:
- `CMakeLists.txt` — build + packaging
- `manifest.json.in` — template manifest (CMake @-substituted)
- `src/main.cpp`, `src/mod/*` — example mod
- `scripts/package.ps1` — local packaging helper (Windows PowerShell). GitHub Actions expected as primary build path.

## 3. Build System

This template uses **CMake + Android NDK** (arm64-v8a/armeabi-v7a). Do **not** assume local NDK is installed — primary workflow is **GitHub Actions** using the built-in mod builder in `scripts/`.

### CMake options/vars (important)

Settable via `-D...` or cache:
- `MOD_ID` — mod package directory name (also used as mod id). Default `clange_me`.
- `MOD_NAME` — display name. Default `Clange Me Mod`.
- `MOD_AUTHOR` — author.
- `MOD_VERSION` — version string (e.g. `1.0.0`).
- `MOD_LIBRARY_NAME` — native lib name **without** `lib` prefix or `.so` suffix (output becomes `lib${MOD_LIBRARY_NAME}.so`). Default `clange_me`.
- `MOD_MINECRAFT_VERSIONS` — raw JSON array for `minecraft_versions` in manifest. For 1.26.52 strictly use `["1.26.52"]`.
- `MOD_ICON` — optional package-relative path (no `..`, no absolute). e.g. `icon.png`.
- `LEVI_PACKAGE_CONFIG_DIR` — dir containing `config.json` and `config.schema.json` to package. Non-Android build generates them to `build-config/generated-config/` via `levi_generate_config` target (config generator). Android build requires them to exist in that dir.
- `LEVI_LINK_PRELOADER` (default ON) — links `preloader` target; else leaves SDK symbols unresolved (runtime lookup).
- `ANDROID_ABI` — `arm64-v8a` or `armeabi-v7a`. Template enforces these two only.
- `ANDROID_PLATFORM` — e.g. `android-28` (PowerShell script uses `android-28`).
- `ANDROID_STL` — `c++_shared` (recommended).

### Build flow (what CMake does)

1. **Non-Android host**: builds `levi_config_generator` (uses `PL_CONFIG_NO_RUNTIME`) and defines `levi_generate_config` custom target to emit `config.json` + `config.schema.json`. Build returns early here (host-only generator).
2. **Android**: requires `LEVI_PACKAGE_CONFIG_DIR` to contain `config.json` and `config.schema.json`. Validates ABI. May add_subdirectory for preloader SDK. Builds `SHARED` lib `${MOD_LIBRARY_NAME}` (output name same). Links: `log`, `nlohmann_json`, `Boost::pfr`, `magic_enum`, `fmt`. If `preloader` target exists links it; else passes `--unresolved-symbols=ignore-all`. Strips with `CMAKE_STRIP` after build (`--strip-unneeded`). Configures `manifest.json.in` → `manifest.json` with @-substitutions. Defines `levi_package` target which:
   - Creates `package/<MOD_ID>/`
   - Copies `$<TARGET_FILE:${MOD_LIBRARY_NAME}>` → `package/<MOD_ID>/lib${MOD_LIBRARY_NAME}.so`
   - Copies `manifest.json` → `package/<MOD_ID>/manifest.json`
   - Copies `config/*` from `LEVI_PACKAGE_CONFIG_DIR` → `package/<MOD_ID>/config/`
   - Copies optional icon (package-relative, validated)
   - Zips `package/<MOD_ID>` as `${MOD_ID}-${MOD_VERSION}-${CMAKE_ANDROID_ARCH_ABI}.levipack` in `build/`

### Packaging script

`scripts/package.ps1` (PowerShell):
- `-Abi` `arm64-v8a|armeabi-v7a|all` (default `arm64-v8a`)
- `-BuildRoot` (default `build`)
- `-AndroidPlatform` (default `android-28`)
- `-NdkHome` (optional; falls back to `ANDROID_HOME/ndk/28.2.13676358`, latest under `ANDROID_HOME/ndk/`, or `ANDROID_NDK_HOME`)
- Runs config generation via host CMake (`levi_generate_config`) to get generated-config dir
- For each ABI: configures with Android toolchain (`android.toolchain.cmake`), sets `-DLEVI_PACKAGE_CONFIG_DIR=<generated-config>`, builds `levi_package`

**Note**: You don't build locally (stated). Use GitHub Actions with this builder pattern. Do not add arbitrary local build scripts unless necessary.

## 4. Mod Loading (LeviLaunchroid) — Where to Put Mods

LeviLaunchroid loads native mods from **Android/media/org.levimc.launcher/**, **not** `com.levi.launcher` and **not** `Android/data/`.

Concrete path (unrooted Android 11+):
```
/storage/emulated/0/Android/media/org.levimc.launcher/minecraft/com.mojang.minecraftpe/mods/<MOD_ID>/
```

For imported/custom versions: `minecraft/<sanitized version dir>/mods/<MOD_ID>/`.

Inside each mod directory:
- `manifest.json` (required)
- `lib<MOD_LIBRARY_NAME>.so` (entry SO). Filename must end with `.so` (case-sensitive `.so`).
- `config/` — config files (writable at runtime)
- `data/` — runtime data (writable at runtime)
- `resources/` — optional resources
- optional `icon` (package-relative as declared)

**Discovery rules (critical):**
- Mods are discovered as **immediate subdirectories** of `mods/` only. One mod per subdirectory. Directory name = **mod id**.
- Must contain `manifest.json` with `"type": "preload-native"`.
- `entry` must point to an existing `.so` file (relative path, no `..`, no leading `/`). Basename ends with `.so`.
- Loose `.so` files directly in `mods/` are **auto-migrated** by LeviLaunchroid into a generated subdirectory with a generated manifest — do **not** rely on that for packaging; always package as `<MOD_ID>/...`.
- Mods are per-version. Order/enabled state stored in `mods/mods_config.json` (managed by launcher).

**Cache**: The launcher copies mods to app cache at `/data/user/0/org.levimc.launcher/cache/native_mods/<version>/mods/<MOD_ID>/` before `dlopen` (files forced read-only), but `getModDir()`, `getDataDir()`, `getConfigDir()` still point to the **original source mod dir** (writable). `data/` and `config/` are **excluded** from cache copy.

## 5. Manifest (`manifest.json`)

Template generates via `manifest.json.in`:
```json
{
  "type": "preload-native",
  "name": "@MOD_NAME@",
  "author": "@MOD_AUTHOR@",
  "version": "@MOD_VERSION@",
  "entry": "lib@MOD_LIBRARY_NAME@.so",
  "icon": "@MOD_ICON@",
  "minecraft_versions": @MOD_MINECRAFT_VERSIONS@
}
```

Required/notes:
- `type` must be `"preload-native"`.
- `entry` is package-relative (e.g. `libclange_me.so`). Path traversal rejected.
- For 1.26.52 strictly, set `@MOD_MINECRAFT_VERSIONS@` to `["1.26.52"]`. You can also use patterns like `"1.26.*"` but version is locked strictly here — prefer exact.
- `icon` can be empty string `""` if unused.

## 6. Entry Point & Lifecycle (C++)

Use the C++ registration API (preferred). **Do not export `JNI_OnLoad`** — preloader owns JNI.

Template does:
```cpp
#include "mod/MyMod.h"
#include <pl/Mod.hpp>
PL_REGISTER_MOD(clange_me::ClangeMeMod, clange_me::ClangeMeMod::instance());
```

Your mod class (template pattern):
```cpp
class ClangeMeMod {
public:
  static ClangeMeMod &instance();
  ClangeMeMod();
  [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }
  bool load(); bool enable(); bool disable(); bool unload();
private:
  ll::mod::NativeMod &mSelf;  // set from NativeMod::current() in ctor
  ...
};
```

- `PL_REGISTER_MOD(TYPE, INSTANCE_EXPR)` generates `extern "C" PL_EXPORT ::pl::mod::ModRegistration *PLGetModRegistration()` and materializes instance once (static). 
- Lifecycle methods can take zero args or `(ModContext&)` depending on signature (concepts allow both). Returning `false` from `load()` aborts registration. `enable/disable/unload` return `bool` (optional in effect but return `true` on success).
- `NativeMod::current()` is `thread_local` and only valid during registration/lifecycle dispatch — capture in constructor as done.
- Directories: `self.getModDir()` (original source), `self.getDataDir()` (`modRoot/data`), `self.getConfigDir()` (`modRoot/config`), `self.getResourceDir()` (`modRoot/resources`). Create them in `load()` as needed.
- Logger: `self.getLogger()` uses tag = manifest `name` (fallback to id). Log levels: `debug/info/warn/error`. Visible via `adb logcat -s <YourModName>`.
- The preloader tries entry points in priority order: **`PLGetModRegistration()`** (preferred) > `PLMod_Load/Enable/Disable/Unload` (C lifecycle) > `LeviMod_Load` (legacy). Use the `PL_REGISTER_MOD` form above.

## 7. Hooking System (Use Preloader's API)

**Do not** mix xhook/Dobby directly. The preloader-android SDK uses **GlossHook** internally (wrapped). Expose the preloader's hooking surface to mods — use the public headers in `include/pl/memory/`.

Public APIs to prefer:

| Header | Namespace | Purpose |
|---|---|---|
| `pl/memory/Hook.hpp` | `pl::memory` | `hook(target, detour, originalFunc, priority)` returns `0` on success, `-1` on failure. `HookHandle` RAII. Priorities `Highest(0)..Lowest(400)`. Multi-detour chain per target (single real hook). |
| `pl/memory/Signature.hpp` | `pl::memory` | `resolveSignature(sig, moduleName)` and `resolveSignatures(signatures, moduleName)`. Dual-mode: tries `dlsym` first (so exported symbol names work), falls back to **Aho-Corasick multi-pattern scan** with nibble-level wildcards (`?`, `?F`, `4?`). Returns `0` on failure. |
| `pl/memory/Vtable.hpp` | `pl::memory` | `resolveVtableFunction(typeInfoName, slot, moduleName)`. Uses RTTI: finds primary vtable (`offsetToTop==0`) in `.data.rel.ro`. Pass e.g. `"14ClientInstance"` or `"_ZTS14ClientInstance"`. |
| `pl/memory/Patch.hpp` | `pl::memory` | `writeBytes(address, bytes/span/hex, name)`, `readBytes`, `revertPatch(name)`, `revertAllPatches()`, `PatchHandle`. Uses mprotect + clear_cache, keyed by **unique name** (silent overwrite on collision). |

**Hook safety rules (must follow):**
- **Always check for null pointers** before dereferencing. If `target==0`/`original==0` after resolution, do not hook.
- **Validate signatures dynamically** — never assume a signature resolves. Check return values (`hook` returns 0/-1; resolvers return 0 on failure).
- **Protect mod updates/state** with appropriate synchronization (mutexes/atomics) and avoid touching game state before it's initialized. Hooks often fire on game threads.
- **Fail gracefully**: if signature/vtable lookup fails, log `warn/error` and skip installing that hook (don't crash).
- **Prefer preloader helpers**. Don't reimplement scanning/hooking. Use batched `resolveSignatures` when resolving many patterns (faster, one pass).
- **Hook priorities matter** if multiple mods detour same target — document reasoning if you rely on order.

**Module names**: For MCPE use `"libminecraftpe.so"`. For system libs use their soname.

## 8. Config System (`pl::config`)

Template uses typed config:
```cpp
struct ModConfig {
  int version = 1;
  bool enabled = true;
  std::string message = "Hello from clange_me";
};
template <> struct pl::config::Schema<ModConfig> {
  static constexpr std::string_view title = "Clange Me Config";
  static constexpr std::string_view description = {};
  static constexpr FieldSchema field(std::string_view name) {
    if (name=="version") return {.title="Version", .readOnly=true};
    ...
    return {};
  }
};
```

Usage:
```cpp
mConfigFile.emplace();
if (!mConfigFile->load()) { /* warn, maybe keep defaults */ }
mConfig = mConfigFile->value();
// later save if modified: mConfigFile->save(mConfig) (atomic temp+rename)
```

`ConfigFile<T>` requires aggregate with integral `version` field (`TypedConfig` concept). Writes are atomic (temp file + rename). Schema JSON is used by Mod Menu/config UI.

Config lives under `modRoot/config/` (packaged from `LEVI_PACKAGE_CONFIG_DIR`). Host generator (`levi_config_generator`) can emit defaults/schema when building non-Android; Android packaging copies pre-existing `config.json` and `config.schema.json` into the `.levipack`.

## 9. Testing (Unrooted Android)

Since device is **not rooted**:
- Mods go in `Android/media/org.levimc.launcher/minecraft/com.mojang.minecraftpe/mods/<MOD_ID>/` (accessible without root; also via ADB on most devices).
- **Do not** expect access to `Android/data/com.levi.launcher/` — that path is not used and is not accessible without root. The correct package is `org.levimc.launcher`.
- Use **wireless ADB/logcat** for logging.

Recommended logcat command (as specified):
```bash
adb logcat -c
adb logcat -v color -s Preloader:* Levi:* MyMod:*
```

Replace `MyMod` with your mod's display/logger name (from manifest `name`). Also useful tags: `ModNativeLoader:*`, `ModManager:*`, `FileHandler:*`.

Useful inspection (run-as works for app's own dirs):
```bash
# Mods tree (source)
adb shell ls -R /storage/emulated/0/Android/media/org.levimc.launcher/minecraft/com.mojang.minecraftpe/mods

# Cache (what's actually dlopen'd)
adb shell run-as org.levimc.launcher ls -R /data/user/0/org.levimc.launcher/cache/native_mods/

# Enabled/order state
adb shell cat /storage/emulated/0/Android/media/org.levimc.launcher/minecraft/com.mojang.minecraftpe/mods/mods_config.json
```

Lifecycle points (for debugging): `load()` runs during native init (pre-game), `enable()` runs in `MinecraftActivity.onCreate()` **before** `super.onCreate()`, `disable()/unload()` run on pause/destroy in reverse load order. Static initializers run when `.so` is loaded (before lifecycle calls) — **do not** touch `libminecraftpe.so` from static initializers (it may not be loaded yet).

## 10. Packaging & Deployment

- **Build path**: GitHub Actions (preferred). Use `scripts/package.ps1` logic or equivalent workflow. Do not clutter local system with build tools.
- **Output**: `.levipack` ZIP produced by `levi_package` target: `build/<MOD_ID>-<MOD_VERSION>-<ABI>.levipack`. Contains the mod directory structure ready for LeviLauncher.
- **Install**: On device, sideload/import the `.levipack` via LeviLauncher, or place extracted `<MOD_ID>/` under the mods dir (see §4). LeviLaunchroid also accepts `.zip`/`.levipack` imports.
- **Manifest version lock**: Set `MOD_VERSION` appropriately and `minecraft_versions` to `["1.26.52"]`. This repo targets 1.26.52 strictly.

## 11. Conventions & Pitfalls

**Conventions (match template):**
- C++20, explicit namespace declarations (template uses `namespace clange_me { ... }`).
- Class fields use `m_` prefixes where applicable (template: `mSelf`, `mConfig`, `mConfigFile`).
- Keep changes minimal, follow existing style. **Do not add comments unless explicitly asked.**
- Use `ll::mod::NativeMod` (backwards-compat alias) as shown; also `pl::mod::*` available.
- Visibility: compiled with `-fvisibility=hidden -fvisibility-inlines-hidden`; only `PL_EXPORT` symbols are callable from outside.

**Pitfalls (Android/LeviLauncher):**
- **arm64-v8a only** is the primary target; Android 11+. 
- **Obfuscated symbols in MCPE**: always use `resolveSignature(s)` with robust patterns or `resolveVtableFunction` for vtables. Never hardcode absolute addresses. 
- **Scoped storage (Android 11+)**: use `Android/media/org.levimc.launcher/` (correct). Do not assume `Android/data/` access. 
- **`JNI_OnLoad`**: owned by preloader — do not define it in your mod.
- **Static initializers**: avoid calling into game libraries at SO load time; do initialization in `load()`.
- **Patch name collisions**: `Patch` operations keyed by name globally; use unique names per mod/patch.
- **hook() return**: `0` = success, `-1` = failure — always check.
- **Signature returns 0 on failure** — validate before use.
- **Manifest re-parsed natively**: Java `Mod` object is ignored; all metadata comes from `manifest.json`. Changes to manifest shape require C++ side awareness if you need new fields exposed (rare).
- **Case-sensitive `.so`** in entry/filename checks.
