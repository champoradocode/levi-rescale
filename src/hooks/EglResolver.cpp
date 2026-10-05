#include "hooks/EglResolver.h"

#include <vector>

#include <pl/memory/Signature.hpp>

namespace levi_rescale {

std::optional<ResolvedEglSymbols> resolveEglSymbols(pl::log::Logger &logger) {
    const std::vector<std::string> signatures = {
        "eglCreateWindowSurface",
        "eglSwapBuffers",
        "eglQuerySurface",
        "eglMakeCurrent",
    };

    const auto resolved = pl::memory::resolveSignatures(signatures, "libEGL.so");
    const auto createIt = resolved.find("eglCreateWindowSurface");
    const auto swapIt = resolved.find("eglSwapBuffers");
    const auto queryIt = resolved.find("eglQuerySurface");
    const auto makeCurrentIt = resolved.find("eglMakeCurrent");

    const uintptr_t create = createIt != resolved.end() ? createIt->second : 0;
    const uintptr_t swap = swapIt != resolved.end() ? swapIt->second : 0;
    const uintptr_t query = queryIt != resolved.end() ? queryIt->second : 0;
    const uintptr_t makeCurrent = makeCurrentIt != resolved.end() ? makeCurrentIt->second : 0;

    logger.info("libEGL.so: eglCreateWindowSurface = 0x{:x}, eglSwapBuffers = 0x{:x}, eglQuerySurface = 0x{:x}, eglMakeCurrent = 0x{:x}",
                static_cast<unsigned long long>(create),
                static_cast<unsigned long long>(swap),
                static_cast<unsigned long long>(query),
                static_cast<unsigned long long>(makeCurrent));

    if (create != 0 && swap != 0 && makeCurrent != 0) {
        return ResolvedEglSymbols{"libEGL.so", create, swap, query, makeCurrent};
    }

    logger.warn("Failed to resolve EGL functions in libEGL.so");
    return std::nullopt;
}

} // namespace levi_rescale
