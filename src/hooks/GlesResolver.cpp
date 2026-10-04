#include "hooks/GlesResolver.h"

#include <string>
#include <vector>

#include <pl/memory/Signature.hpp>

namespace levi_rescale {

std::optional<ResolvedGlesSymbols> resolveGlesSymbols(pl::log::Logger &logger) {
    const std::vector<std::string> candidates = {
        "libGLESv3.so",
        "libGLESv2.so",
        "libGLESv1_CM.so",
    };
    const std::vector<std::string> signatures = {
        "glViewport",
        "glScissor",
    };

    for (const auto &module : candidates) {
        const auto resolved = pl::memory::resolveSignatures(signatures, module);
        const auto viewportIt = resolved.find("glViewport");
        const auto scissorIt = resolved.find("glScissor");

        const uintptr_t glViewport =
            viewportIt != resolved.end() ? viewportIt->second : 0;
        const uintptr_t glScissor =
            scissorIt != resolved.end() ? scissorIt->second : 0;

        logger.info("GLES candidate '{}': glViewport = 0x{:x}, glScissor = 0x{:x}",
                    module,
                    static_cast<unsigned long long>(glViewport),
                    static_cast<unsigned long long>(glScissor));

        if (glViewport != 0 && glScissor != 0) {
            logger.info("Resolved GL viewport functions in '{}'", module);
            return ResolvedGlesSymbols{module, glViewport, glScissor};
        }
    }

    logger.warn("No candidate GLES module provided both glViewport and glScissor");
    return std::nullopt;
}

} // namespace levi_rescale
