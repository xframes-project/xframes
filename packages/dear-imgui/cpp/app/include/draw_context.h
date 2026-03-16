#pragma once

#include <imgui.h>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

#include "csscolorparser.hpp"

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
    std::unordered_map<std::string, ImU32> colorCache;
};

// Parse CSS color string (e.g. "#FF0000", "red") to ImU32.
// Calls CSSColorParser directly — no JSON wrapper, no float round-trip.
// Optional DrawContext* enables per-widget color cache (most scripts use <10 distinct colors).
inline ImU32 parseCSSColor(const char* colorStr, DrawContext* dc = nullptr) {
    if (dc) {
        auto it = dc->colorCache.find(colorStr);
        if (it != dc->colorCache.end()) return it->second;
    }

    auto maybeColor = CSSColorParser::parse(std::string(colorStr));
    if (!maybeColor.has_value()) return IM_COL32(255, 255, 255, 255);

    const auto& c = maybeColor.value();
    ImU32 result = IM_COL32(c.r, c.g, c.b, static_cast<unsigned char>(c.a * 255.0f));

    if (dc) {
        dc->colorCache[colorStr] = result;
    }
    return result;
}
