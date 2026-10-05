#include "mod/RescaleMod.h"

#include <filesystem>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "hooks/GlesResolver.h"
#include "hooks/EglResolver.h"
#include "hooks/GlFboResolver.h"

#include <pl/Mod.hpp>

namespace levi_rescale {

using EglQuerySurfaceFn = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint, EGLint *);
using EglCreateWindowSurfaceFn =
    EGLSurface (*)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *);
using EglSwapBuffersFn = EGLBoolean (*)(EGLDisplay, EGLSurface);
using EglMakeCurrentFn = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);

static void *gOriginalEglQuerySurfaceRaw = nullptr;
static void *gOriginalCreateSurfaceRaw = nullptr;
static void *gOriginalSwapBuffersRaw = nullptr;
static void *gOriginalBindFramebufferRaw = nullptr;
static void *gOriginalGlBlitFramebufferRaw = nullptr;
static void *gOriginalViewportRaw = nullptr;
static bool gInBlit = false;
static EGLSurface gCurrentDrawSurface = nullptr;
static EGLSurface gMainEglSurface = nullptr;
static void *gOriginalMakeCurrentRaw = nullptr;

using GlGenFramebuffersFn = void (*)(GLsizei, GLuint *);
using GlBindFramebufferFn = void (*)(GLenum, GLuint);
using GlFramebufferTexture2DFn = void (*)(GLenum, GLenum, GLenum, GLuint, GLint);
using GlGenTexturesFn = void (*)(GLsizei, GLuint *);
using GlBindTextureFn = void (*)(GLenum, GLuint);
using GlTexImage2DFn = void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
using GlCheckFramebufferStatusFn = GLenum (*)(GLenum);
using GlBlitFramebufferFn = void (*)(GLint, GLint, GLint, GLint,
                                     GLint, GLint, GLint, GLint,
                                     GLenum, GLenum);
using GlViewportFn = void (*)(int, int, int, int);

static GlGenFramebuffersFn gGlGenFramebuffers = nullptr;
static GlBindFramebufferFn gGlBindFramebuffer = nullptr;
static GlFramebufferTexture2DFn gGlFramebufferTexture2D = nullptr;
static GlGenTexturesFn gGlGenTextures = nullptr;
static GlBindTextureFn gGlBindTexture = nullptr;
static GlTexImage2DFn gGlTexImage2D = nullptr;
static GlCheckFramebufferStatusFn gGlCheckFramebufferStatus = nullptr;

static GLuint gSmallFbo = 0;
static GLuint gSmallTexture = 0;
static int gScreenWidth = 0;
static int gScreenHeight = 0;
static int gSmallWidth = 0;
static int gSmallHeight = 0;
static bool gFboReady = false;

static EGLSurface createSurfaceDetour(EGLDisplay dpy, EGLConfig cfg,
                                      EGLNativeWindowType win, const EGLint *attribs) {
    EGLSurface surface = nullptr;
    auto create = reinterpret_cast<EglCreateWindowSurfaceFn>(gOriginalCreateSurfaceRaw);

    if (create) {
        surface = create(dpy, cfg, win, attribs);
    }

    if (surface != EGL_NO_SURFACE && gOriginalEglQuerySurfaceRaw) {
        auto query = reinterpret_cast<EglQuerySurfaceFn>(gOriginalEglQuerySurfaceRaw);
        EGLint w = 0;
        EGLint h = 0;
        query(dpy, surface, EGL_WIDTH, &w);
        query(dpy, surface, EGL_HEIGHT, &h);
        if (w >= 1000 && h >= 400) {
            gScreenWidth = w;
            gScreenHeight = h;
            gSmallWidth = w / 2;
            gSmallHeight = h / 2;
            gMainEglSurface = surface;
        }
    }

    return surface;
}

static EGLBoolean swapBuffersDetour(EGLDisplay dpy, EGLSurface surface) {
    static bool sLoggedSwapBuffers = false;
    if (!sLoggedSwapBuffers) {
        sLoggedSwapBuffers = true;
        RescaleMod::instance().getSelf().getLogger().info("eglSwapBuffers called");
    }

    bool isMainSurface = false;
    if (gOriginalEglQuerySurfaceRaw != nullptr) {
        auto query = reinterpret_cast<EglQuerySurfaceFn>(gOriginalEglQuerySurfaceRaw);
        EGLint w = 0;
        EGLint h = 0;
        query(dpy, surface, EGL_WIDTH, &w);
        query(dpy, surface, EGL_HEIGHT, &h);

        if (w >= 1000 && h >= 400) {
            isMainSurface = true;
            gMainEglSurface = surface;
            gScreenWidth = w;
            gScreenHeight = h;
            gSmallWidth = w / 2;
            gSmallHeight = h / 2;
        }
    }

#if 0
    if (!gFboReady && isMainSurface && gGlGenTextures && gGlBindTexture && gGlTexImage2D &&
        gGlGenFramebuffers && gGlBindFramebuffer && gGlFramebufferTexture2D) {
        gGlGenTextures(1, &gSmallTexture);
        gGlBindTexture(GL_TEXTURE_2D, gSmallTexture);
        gGlTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gSmallWidth, gSmallHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        gGlGenFramebuffers(1, &gSmallFbo);
        gGlBindFramebuffer(GL_FRAMEBUFFER, gSmallFbo);
        gGlFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gSmallTexture, 0);

        if (gGlCheckFramebufferStatus) {
            const GLenum status = gGlCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                RescaleMod::instance().getSelf().getLogger().error(
                    "Small FBO incomplete: 0x{:x}", static_cast<unsigned int>(status));
            } else {
                gFboReady = true;
                RescaleMod::instance().getSelf().getLogger().info(
                    "Created small FBO: {}x{}", gSmallWidth, gSmallHeight);
            }
        } else {
            gFboReady = true;
            RescaleMod::instance().getSelf().getLogger().info(
                "Created small FBO: {}x{}", gSmallWidth, gSmallHeight);
        }
    }

    if (gFboReady && isMainSurface && gOriginalGlBlitFramebufferRaw && gOriginalBindFramebufferRaw) {
        gInBlit = true;

        auto originalBind = reinterpret_cast<GlBindFramebufferFn>(gOriginalBindFramebufferRaw);
        auto blit = reinterpret_cast<GlBlitFramebufferFn>(gOriginalGlBlitFramebufferRaw);

        originalBind(GL_READ_FRAMEBUFFER, gSmallFbo);
        originalBind(GL_DRAW_FRAMEBUFFER, 0);
        blit(0, 0, gSmallWidth, gSmallHeight,
             0, 0, gScreenWidth, gScreenHeight,
             GL_COLOR_BUFFER_BIT, GL_LINEAR);

        gInBlit = false;
    }
#endif

    auto swap = reinterpret_cast<EglSwapBuffersFn>(gOriginalSwapBuffersRaw);
    if (swap) {
        return swap(dpy, surface);
    }
    return EGL_FALSE;
}

static EGLBoolean makeCurrentDetour(EGLDisplay dpy, EGLSurface draw,
                                    EGLSurface read, EGLContext ctx) {
    auto makeCurrent = reinterpret_cast<EglMakeCurrentFn>(gOriginalMakeCurrentRaw);
    const EGLBoolean ok = makeCurrent != nullptr ? makeCurrent(dpy, draw, read, ctx) : EGL_FALSE;

    if (ok == EGL_TRUE) {
        gCurrentDrawSurface = draw;
    }

    return ok;
}

static void glBindFramebufferDetour(GLenum target, GLuint framebuffer) {
    auto original = reinterpret_cast<GlBindFramebufferFn>(gOriginalBindFramebufferRaw);

    if (!gInBlit && gFboReady && framebuffer == 0 && target == GL_FRAMEBUFFER &&
        gCurrentDrawSurface == gMainEglSurface) {
        static bool sLoggedBindRedirect = false;
        if (!sLoggedBindRedirect) {
            sLoggedBindRedirect = true;
            RescaleMod::instance().getSelf().getLogger().info(
                "Redirected default FBO to small FBO");
        }

        if (original) {
            original(target, gSmallFbo);
        }
        return;
    }

    if (original) {
        original(target, framebuffer);
    }
}

static void viewportDetour(int x, int y, int width, int height) {
    static int sLastX = -1;
    static int sLastY = -1;
    static int sLastWidth = -1;
    static int sLastHeight = -1;

    if (x != sLastX || y != sLastY || width != sLastWidth || height != sLastHeight) {
        sLastX = x;
        sLastY = y;
        sLastWidth = width;
        sLastHeight = height;
        RescaleMod::instance().getSelf().getLogger().info(
            "glViewport input: {}x{} at ({}, {})", width, height, x, y);
    }

    if (gFboReady && gSmallWidth > 0 && gSmallHeight > 0 &&
        width > gSmallWidth && height > gSmallHeight) {
        width = gSmallWidth;
        height = gSmallHeight;
    }

    auto original = reinterpret_cast<GlViewportFn>(gOriginalViewportRaw);
    if (original) {
        original(x, y, width, height);
    }
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

    if (mGlesSymbols) {
        mGlFboSymbols = resolveGlFboSymbols(self.getLogger(), mGlesSymbols->module);
        if (!mGlFboSymbols) {
            self.getLogger().warn("GL FBO symbols unavailable; FBO hooking will be skipped");
        }
    }

    if (mGlFboSymbols) {
        gGlGenFramebuffers = reinterpret_cast<GlGenFramebuffersFn>(mGlFboSymbols->glGenFramebuffers);
        gGlBindFramebuffer = reinterpret_cast<GlBindFramebufferFn>(mGlFboSymbols->glBindFramebuffer);
        gGlFramebufferTexture2D = reinterpret_cast<GlFramebufferTexture2DFn>(mGlFboSymbols->glFramebufferTexture2D);
        gGlGenTextures = reinterpret_cast<GlGenTexturesFn>(mGlFboSymbols->glGenTextures);
        gGlBindTexture = reinterpret_cast<GlBindTextureFn>(mGlFboSymbols->glBindTexture);
        gGlTexImage2D = reinterpret_cast<GlTexImage2DFn>(mGlFboSymbols->glTexImage2D);
        gGlCheckFramebufferStatus = mGlFboSymbols->glCheckFramebufferStatus != 0
            ? reinterpret_cast<GlCheckFramebufferStatusFn>(mGlFboSymbols->glCheckFramebufferStatus)
            : nullptr;
        gOriginalGlBlitFramebufferRaw = mGlFboSymbols->glBlitFramebuffer != 0
            ? reinterpret_cast<void *>(mGlFboSymbols->glBlitFramebuffer)
            : nullptr;
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

    if (mEglSymbols->makeCurrent != 0) {
        gOriginalMakeCurrentRaw = reinterpret_cast<void *>(mEglSymbols->makeCurrent);
        mEglMakeCurrentHook.emplace(reinterpret_cast<pl::memory::FuncPtr>(mEglSymbols->makeCurrent),
                                    reinterpret_cast<pl::memory::FuncPtr>(&makeCurrentDetour),
                                    &gOriginalMakeCurrentRaw,
                                    pl::memory::HookPriority::Normal);
        if (!mEglMakeCurrentHook->installed()) {
            self.getLogger().error("Failed to hook eglMakeCurrent");
        }
    }

    if (mGlFboSymbols && gGlBindFramebuffer) {
        mGlBindFramebufferHook.emplace(reinterpret_cast<pl::memory::FuncPtr>(mGlFboSymbols->glBindFramebuffer),
                                       reinterpret_cast<pl::memory::FuncPtr>(&glBindFramebufferDetour),
                                       &gOriginalBindFramebufferRaw,
                                       pl::memory::HookPriority::Normal);
        if (!mGlBindFramebufferHook->installed()) {
            self.getLogger().error("Failed to hook glBindFramebuffer");
        }
    }

    if (mGlesSymbols && mGlesSymbols->glViewport != 0) {
        mViewportHook.emplace(reinterpret_cast<pl::memory::FuncPtr>(mGlesSymbols->glViewport),
                              reinterpret_cast<pl::memory::FuncPtr>(&viewportDetour),
                              &gOriginalViewportRaw,
                              pl::memory::HookPriority::Normal);
        if (!mViewportHook->installed()) {
            self.getLogger().error("Failed to hook glViewport");
        }
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
    if (mEglMakeCurrentHook) {
        mEglMakeCurrentHook->reset();
        mEglMakeCurrentHook.reset();
    }
    if (mGlBindFramebufferHook) {
        mGlBindFramebufferHook->reset();
        mGlBindFramebufferHook.reset();
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
