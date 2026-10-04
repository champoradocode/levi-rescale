#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <pl/Logger.hpp>

namespace levi_rescale {

struct ResolvedGlesSymbols {
    std::string module;
    uintptr_t glViewport = 0;
    uintptr_t glScissor = 0;
};

std::optional<ResolvedGlesSymbols> resolveGlesSymbols(pl::log::Logger &logger,
                                                      std::string_view preferredModule);

} // namespace levi_rescale
