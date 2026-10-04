#pragma once

#include <optional>

#include "mod/Config.h"
#include "hooks/GlesResolver.h"
#include "hooks/EglResolver.h"

#include <pl/Mod.hpp>

namespace levi_rescale {

class RescaleMod {
  public:
    static RescaleMod &instance();

    RescaleMod();

    [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

  private:
    ll::mod::NativeMod &mSelf;
    ModConfig mConfig;
    std::optional<pl::config::ConfigFile<ModConfig>> mConfigFile;
    std::optional<ResolvedGlesSymbols> mGlesSymbols;
    std::optional<ResolvedEglSymbols> mEglSymbols;
  };

} // namespace levi_rescale
