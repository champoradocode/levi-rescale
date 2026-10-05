#include "hooks/GlFboResolver.h"

#include <string>
#include <vector>

#include <pl/memory/Signature.hpp>

namespace levi_rescale {

std::optional<ResolvedGlFboSymbols>
resolveGlFboSymbols(pl::log::Logger &logger, std::string_view module) {
    const std::vector<std::string> signatures = {
        "glGenFramebuffers",
        "glBindFramebuffer",
        "glFramebufferTexture2D",
        "glGenTextures",
        "glBindTexture",
        "glTexImage2D",
        "glBlitFramebuffer",
        "glViewport",
        "glCheckFramebufferStatus",
    };

    const auto resolved = pl::memory::resolveSignatures(signatures, module);

    auto get = [&](std::string_view name) {
        auto it = resolved.find(std::string(name));
        return it != resolved.end() ? it->second : 0;
    };

    ResolvedGlFboSymbols out{};
    out.module = std::string(module);
    out.glGenFramebuffers = get("glGenFramebuffers");
    out.glBindFramebuffer = get("glBindFramebuffer");
    out.glFramebufferTexture2D = get("glFramebufferTexture2D");
    out.glGenTextures = get("glGenTextures");
    out.glBindTexture = get("glBindTexture");
    out.glTexImage2D = get("glTexImage2D");
    out.glBlitFramebuffer = get("glBlitFramebuffer");
    out.glViewport = get("glViewport");
    out.glCheckFramebufferStatus = get("glCheckFramebufferStatus");

    logger.info("{}: glGenFramebuffers=0x{:x}, glBindFramebuffer=0x{:x}, glFramebufferTexture2D=0x{:x}, glGenTextures=0x{:x}, glBindTexture=0x{:x}, glTexImage2D=0x{:x}, glBlitFramebuffer=0x{:x}, glViewport=0x{:x}, glCheckFramebufferStatus=0x{:x}",
                std::string(module),
                static_cast<unsigned long long>(out.glGenFramebuffers),
                static_cast<unsigned long long>(out.glBindFramebuffer),
                static_cast<unsigned long long>(out.glFramebufferTexture2D),
                static_cast<unsigned long long>(out.glGenTextures),
                static_cast<unsigned long long>(out.glBindTexture),
                static_cast<unsigned long long>(out.glTexImage2D),
                static_cast<unsigned long long>(out.glBlitFramebuffer),
                static_cast<unsigned long long>(out.glViewport),
                static_cast<unsigned long long>(out.glCheckFramebufferStatus));

    const bool ok = out.glGenFramebuffers && out.glBindFramebuffer &&
                    out.glFramebufferTexture2D && out.glGenTextures &&
                    out.glBindTexture && out.glTexImage2D && out.glBlitFramebuffer &&
                    out.glViewport && out.glCheckFramebufferStatus;

    if (ok) {
        return out;
    }

    logger.warn("{}: some GL FBO functions could not be resolved", std::string(module));
    return std::nullopt;
}

} // namespace levi_rescale
