#include "mod/RescaleMod.h"

#include <filesystem>

#include "hooks/GlesResolver.h"

#include <pl/Mod.hpp>
#include <pl/memory/Hook.hpp>

namespace levi_rescale {

using GlViewportFn = void (*)(int x, int y, int width, int height);
static void *g_originalViewport = nullptr;

static void viewportDetour(int x, int y, int width, int height) {
    static int sLastWidth = -1;
    static int sLastHeight = -1;

    if (width != sLastWidth || height != sLastHeight) {
        sLastWidth = width;
        sLastHeight = height;
        RescaleMod::instance().getSelf().getLogger().info("Game viewport: {}x{}", width, height);
    }

    if (g_originalViewport) {
        auto original = reinterpret_cast<GlViewportFn>(g_originalViewport);
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
        return true;
    }

    mViewportHook.emplace(reinterpret_cast<pl::memory::FuncPtr>(mGlesSymbols->glViewport),
                          reinterpret_cast<pl::memory::FuncPtr>(&viewportDetour),
                          &g_originalViewport,
                          pl::memory::HookPriority::Normal);

    if (!mViewportHook->installed()) {
        self.getLogger().error("Failed to install viewport hook");
    }
    return true;
}

bool RescaleMod::disable() {
    getSelf().getLogger().debug("Disabling...");
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
