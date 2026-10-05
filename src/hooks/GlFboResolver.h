#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <pl/Logger.hpp>

namespace levi_rescale {

struct ResolvedGlFboSymbols {
    std::string module;
    uintptr_t glGenFramebuffers = 0;
    uintptr_t glBindFramebuffer = 0;
    uintptr_t glFramebufferTexture2D = 0;
    uintptr_t glGenTextures = 0;
    uintptr_t glBindTexture = 0;
    uintptr_t glTexImage2D = 0;
    uintptr_t glBlitFramebuffer = 0;
    uintptr_t glViewport = 0;
    uintptr_t glCheckFramebufferStatus = 0;
};

std::optional<ResolvedGlFboSymbols>
resolveGlFboSymbols(pl::log::Logger &logger, std::string_view module);

} // namespace levi_rescale
