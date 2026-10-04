#include "mod/RescaleMod.h"

#include <filesystem>

#include "hooks/GlesResolver.h"
#include "hooks/EglResolver.h"

#include <pl/Mod.hpp>

namespace levi_rescale {

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
    }
    return true;
}

bool RescaleMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    return true;
}

bool RescaleMod::unload() {
    getSelf().getLogger().debug("Unloading...");
    // Release load-time resources here.
    mConfigFile.reset();
    return true;
}

} // namespace levi_rescale
