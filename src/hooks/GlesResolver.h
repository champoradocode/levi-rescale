#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <pl/Logger.hpp>

namespace levi_rescale {

struct ResolvedGlesSymbols {
    std::string module;
    uintptr_t glViewport = 0;
    uintptr_t glScissor = 0;
};

std::optional<ResolvedGlesSymbols> resolveGlesSymbols(pl::log::Logger &logger);

} // namespace levi_rescale
