#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <pl/Logger.hpp>

namespace levi_rescale {

struct ResolvedEglSymbols {
    std::string module;
    uintptr_t createWindowSurface = 0;
    uintptr_t swapBuffers = 0;
    uintptr_t querySurface = 0;
};

std::optional<ResolvedEglSymbols> resolveEglSymbols(pl::log::Logger &logger);

} // namespace levi_rescale
