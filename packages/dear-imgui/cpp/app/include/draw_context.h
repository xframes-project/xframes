#pragma once

#include <imgui.h>
#include <vector>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

#include "color_helpers.h"

using json = nlohmann::json;

// Bridge between scripted draw calls and ImDrawList.
// In production, drawList points to the JsCanvas/LuaCanvas widget's ImDrawList (set per-frame).
// In tests, drawList is null and recording captures call parameters.
struct DrawContext {
    ImDrawList* drawList = nullptr;
    ImVec2 offset = {0, 0};
    ImFont* currentFont = nullptr; // Set per-frame in JsCanvas/LuaCanvas::Render() for text measurement

    struct DrawCall {
        std::string function;
        std::vector<float> floatArgs;
        ImU32 color = 0;
        std::string textArg;
    };
    std::vector<DrawCall> recorded;
    bool recording = false;
    std::function<ImTextureID(const std::string&)> textureLookup;
};

// Parse CSS color string (e.g. "#FF0000", "red") to ImU32.
// Uses extractColor() from color_helpers.h + pure ImGui math (no context needed).
inline ImU32 parseCSSColor(const char* colorStr) {
    json colorJson = colorStr;
    auto maybeColor = extractColor(colorJson);
    if (!maybeColor.has_value()) return IM_COL32(255, 255, 255, 255);
    return ImGui::ColorConvertFloat4ToU32(maybeColor.value());
}
