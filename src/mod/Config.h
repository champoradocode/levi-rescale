#pragma once

#include <string>
#include <string_view>

#include <pl/Config.hpp>

namespace levi_rescale {

struct ModConfig {
    int version = 1;
    bool enabled = true;
    std::string message = "Hello from clange_me";
};

nlohmann::json makeDefaultConfigJson();
nlohmann::json makeConfigSchemaJson();

} // namespace levi_rescale

template <> struct pl::config::Schema<levi_rescale::ModConfig> {
    static constexpr std::string_view title = "Clange Me Config";
    static constexpr std::string_view description = {};

    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version")
            return {.title = "Version", .readOnly = true};
        if (name == "enabled")
            return {.title = "Enabled", .description = "Turns clange_me behavior on or off."};
        if (name == "message")
            return {.title = "Message", .description = "Message written when the mod is enabled."};
        return {};
    }
};
