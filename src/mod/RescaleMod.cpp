#include "mod/RescaleMod.h"

#include <filesystem>

#include <EGL/egl.h>
#include <android/native_window.h>

#include "hooks/GlesResolver.h"
#include "hooks/EglResolver.h"

#include <pl/Mod.hpp>

namespace levi_rescale {

using EglQuerySurfaceFn = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint, EGLint *);
using EglCreateWindowSurfaceFn =
    EGLSurface (*)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *);
using EglSwapBuffersFn = EGLBoolean (*)(EGLDisplay, EGLSurface);

static void *gOriginalEglQuerySurfaceRaw = nullptr;
static void *gOriginalCreateSurfaceRaw = nullptr;
static void *gOriginalSwapBuffersRaw = nullptr;
static void *gOriginalViewportRaw = nullptr;
static bool gNeedLogViewportAfterEgl = false;
static int gLowresWidth = 0;
static int gLowresHeight = 0;
static bool gLoggedLowresViewport = false;

using GlViewportFn = void (*)(int x, int y, int width, int height);

static void viewportDetour(int x, int y, int width, int height) {
    if (gNeedLogViewportAfterEgl && width > 1 && height > 1) {
        gNeedLogViewportAfterEgl = false;
        RescaleMod::instance().getSelf().getLogger().info(
            "glViewport after EGL surface: {}x{}", width, height);
    }

    if (gLowresWidth > 0 && gLowresHeight > 0 &&
        width > gLowresWidth && height > gLowresHeight) {
        if (!gLoggedLowresViewport) {
            gLoggedLowresViewport = true;
            RescaleMod::instance().getSelf().getLogger().info(
                "Forcing glViewport {}x{} -> {}x{}", width, height, gLowresWidth, gLowresHeight);
        }
        width = gLowresWidth;
        height = gLowresHeight;
    }

    auto original = reinterpret_cast<GlViewportFn>(gOriginalViewportRaw);
    if (original) {
        original(x, y, width, height);
    }
}

static EGLSurface createSurfaceDetour(EGLDisplay dpy, EGLConfig cfg,
                                      EGLNativeWindowType win, const EGLint *attribs) {
    EGLSurface surface = nullptr;
    auto create = reinterpret_cast<EglCreateWindowSurfaceFn>(gOriginalCreateSurfaceRaw);
    if (win) {
        auto *nativeWindow = static_cast<ANativeWindow *>(win);
        ANativeWindow_setBuffersGeometry(nativeWindow, 1200, 540, 0);
    }
    if (create) {
        surface = create(dpy, cfg, win, attribs);
    }

    if (surface != EGL_NO_SURFACE && gOriginalEglQuerySurfaceRaw) {
        auto query = reinterpret_cast<EglQuerySurfaceFn>(gOriginalEglQuerySurfaceRaw);
        EGLint w = 0;
        EGLint h = 0;
        query(dpy, surface, EGL_WIDTH, &w);
        query(dpy, surface, EGL_HEIGHT, &h);
        RescaleMod::instance().getSelf().getLogger().info("EGL surface: {}x{}", w, h);
        gNeedLogViewportAfterEgl = true;
        gLowresWidth = w;
        gLowresHeight = h;
    }

    return surface;
}

static EGLBoolean swapBuffersDetour(EGLDisplay dpy, EGLSurface surface) {
    static bool sLoggedSwapBuffers = false;
    if (!sLoggedSwapBuffers) {
        sLoggedSwapBuffers = true;
        RescaleMod::instance().getSelf().getLogger().info("eglSwapBuffers called");
    }

    auto swap = reinterpret_cast<EglSwapBuffersFn>(gOriginalSwapBuffersRaw);
    if (swap) {
        return swap(dpy, surface);
    }
    return EGL_FALSE;
}

RescaleMod &RescaleMod::instance() {
    static RescaleMod instance;
    return instance;
}

RescaleMod::RescaleMod() : mSelf(*ll::mod::NativeMod::current()) {}

bool RescaleMod::load() {
    auto &self = getSelf();
    self.getLogger().debug("Loading...");

    std::error_code ec;
    std::filesystem::create_directories(self.getDataDir(), ec);
    if (ec) {
        self.getLogger().error("Failed to create data directory {}: {}", self.getDataDir().string(),
                               ec.message());
        return false;
    }

    std::filesystem::create_directories(self.getConfigDir(), ec);
    if (ec) {
        self.getLogger().error("Failed to create config directory {}: {}",
                               self.getConfigDir().string(), ec.message());
        return false;
    }

    mConfigFile.emplace();
    if (!mConfigFile->load()) {
        self.getLogger().warn("Failed to load typed config");
        return false;
    }
    mConfig = mConfigFile->value();

    self.getLogger().info("Loaded {} from {}", self.getName(), self.getModDir().string());
    return true;
}

bool RescaleMod::enable() {
    auto &self = getSelf();
    self.getLogger().debug("Enabling...");
    if (!mConfig.enabled) {
        self.getLogger().info("Levi-ReScale is disabled by config");
        return true;
    }

    self.getLogger().info("Config message: {}", mConfig.message);

    mGlesSymbols = resolveGlesSymbols(self.getLogger(), mConfig.preferred_gles_module);
    if (!mGlesSymbols) {
        self.getLogger().warn("GL viewport functions unavailable; later GL hooks will be skipped");
    }

    mEglSymbols = resolveEglSymbols(self.getLogger());
    if (!mEglSymbols) {
        self.getLogger().warn("EGL functions unavailable; later EGL hooks will be skipped");
        return true;
    }

    gOriginalEglQuerySurfaceRaw = mEglSymbols->querySurface != 0
        ? reinterpret_cast<void *>(mEglSymbols->querySurface)
        : nullptr;

    mEglCreateSurfaceHook.emplace(reinterpret_cast<pl::memory::FuncPtr>(mEglSymbols->createWindowSurface),
                                  reinterpret_cast<pl::memory::FuncPtr>(&createSurfaceDetour),
                                  &gOriginalCreateSurfaceRaw,
                                  pl::memory::HookPriority::Normal);
    if (!mEglCreateSurfaceHook->installed()) {
        self.getLogger().error("Failed to hook eglCreateWindowSurface");
    }

    mEglSwapBuffersHook.emplace(reinterpret_cast<pl::memory::FuncPtr>(mEglSymbols->swapBuffers),
                                reinterpret_cast<pl::memory::FuncPtr>(&swapBuffersDetour),
                                &gOriginalSwapBuffersRaw,
                                pl::memory::HookPriority::Normal);
    if (!mEglSwapBuffersHook->installed()) {
        self.getLogger().error("Failed to hook eglSwapBuffers");
    }

    if (mGlesSymbols) {
        mViewportHook.emplace(reinterpret_cast<pl::memory::FuncPtr>(mGlesSymbols->glViewport),
                              reinterpret_cast<pl::memory::FuncPtr>(&viewportDetour),
                              &gOriginalViewportRaw,
                              pl::memory::HookPriority::Normal);
        if (!mViewportHook->installed()) {
            self.getLogger().error("Failed to hook glViewport for diagnostic logging");
        }
    } else {
        self.getLogger().warn("Skipping viewport diagnostic hook because GLES symbols are unavailable");
    }

    return true;
}

bool RescaleMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    if (mEglCreateSurfaceHook) {
        mEglCreateSurfaceHook->reset();
        mEglCreateSurfaceHook.reset();
    }
    if (mEglSwapBuffersHook) {
        mEglSwapBuffersHook->reset();
        mEglSwapBuffersHook.reset();
    }
    if (mViewportHook) {
        mViewportHook->reset();
        mViewportHook.reset();
    }
    return true;
}

bool RescaleMod::unload() {
    getSelf().getLogger().debug("Unloading...");
    // Release load-time resources here.
    mConfigFile.reset();
    return true;
}

} // namespace levi_rescale
