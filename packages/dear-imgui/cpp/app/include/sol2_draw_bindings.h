#pragma once

#include <sol/sol.hpp>
#include "draw_context.h"

namespace Sol2DrawBindings {

inline void registerDrawBindings(sol::state& lua, DrawContext& dc) {

    // drawLine(x1, y1, x2, y2, color, thickness?)
    lua.set_function("drawLine", [&dc](
        float x1, float y1, float x2, float y2,
        const std::string& color, sol::optional<float> thickness
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float t = thickness.value_or(1.0f);
        float ox = dc.offset.x, oy = dc.offset.y;
        if (dc.recording) {
            dc.recorded.push_back({"drawLine",
                {x1 + ox, y1 + oy, x2 + ox, y2 + oy, t}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddLine({x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, col, t);
        }
    });

    // drawRect(x, y, w, h, color, thickness?, rounding?)
    lua.set_function("drawRect", [&dc](
        float x, float y, float w, float h,
        const std::string& color, sol::optional<float> thickness, sol::optional<float> rounding
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float t = thickness.value_or(1.0f);
        float r = rounding.value_or(0.0f);
        float ox = dc.offset.x, oy = dc.offset.y;
        if (dc.recording) {
            dc.recorded.push_back({"drawRect",
                {x + ox, y + oy, w, h, t, r}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddRect({x + ox, y + oy}, {x + w + ox, y + h + oy}, col, r, 0, t);
        }
    });

    // drawRectFilled(x, y, w, h, color, rounding?)
    lua.set_function("drawRectFilled", [&dc](
        float x, float y, float w, float h,
        const std::string& color, sol::optional<float> rounding
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float r = rounding.value_or(0.0f);
        float ox = dc.offset.x, oy = dc.offset.y;
        if (dc.recording) {
            dc.recorded.push_back({"drawRectFilled",
                {x + ox, y + oy, w, h, r}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddRectFilled({x + ox, y + oy}, {x + w + ox, y + h + oy}, col, r);
        }
    });

    // drawCircle(cx, cy, radius, color, thickness?, segments?)
    lua.set_function("drawCircle", [&dc](
        float cx, float cy, float radius,
        const std::string& color, sol::optional<float> thickness, sol::optional<int> segments
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float t = thickness.value_or(1.0f);
        int seg = segments.value_or(0);
        if (dc.recording) {
            dc.recorded.push_back({"drawCircle",
                {cx + dc.offset.x, cy + dc.offset.y, radius, t, (float)seg}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddCircle({cx + dc.offset.x, cy + dc.offset.y}, radius, col, seg, t);
        }
    });

    // drawCircleFilled(cx, cy, radius, color, segments?)
    lua.set_function("drawCircleFilled", [&dc](
        float cx, float cy, float radius,
        const std::string& color, sol::optional<int> segments
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        int seg = segments.value_or(0);
        if (dc.recording) {
            dc.recorded.push_back({"drawCircleFilled",
                {cx + dc.offset.x, cy + dc.offset.y, radius, (float)seg}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddCircleFilled({cx + dc.offset.x, cy + dc.offset.y}, radius, col, seg);
        }
    });

    // drawTriangle(x1, y1, x2, y2, x3, y3, color, thickness?)
    lua.set_function("drawTriangle", [&dc](
        float x1, float y1, float x2, float y2, float x3, float y3,
        const std::string& color, sol::optional<float> thickness
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float t = thickness.value_or(1.0f);
        float ox = dc.offset.x, oy = dc.offset.y;
        if (dc.recording) {
            dc.recorded.push_back({"drawTriangle",
                {x1 + ox, y1 + oy, x2 + ox, y2 + oy, x3 + ox, y3 + oy, t}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddTriangle(
                {x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, {x3 + ox, y3 + oy}, col, t);
        }
    });

    // drawTriangleFilled(x1, y1, x2, y2, x3, y3, color)
    lua.set_function("drawTriangleFilled", [&dc](
        float x1, float y1, float x2, float y2, float x3, float y3,
        const std::string& color
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float ox = dc.offset.x, oy = dc.offset.y;
        if (dc.recording) {
            dc.recorded.push_back({"drawTriangleFilled",
                {x1 + ox, y1 + oy, x2 + ox, y2 + oy, x3 + ox, y3 + oy}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddTriangleFilled(
                {x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, {x3 + ox, y3 + oy}, col);
        }
    });

    // drawText(x, y, color, text)
    lua.set_function("drawText", [&dc](
        float x, float y, const std::string& color, const std::string& text
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        if (dc.recording) {
            dc.recorded.push_back({"drawText",
                {x + dc.offset.x, y + dc.offset.y}, col, text});
        }
        if (dc.drawList) {
            dc.drawList->AddText({x + dc.offset.x, y + dc.offset.y}, col, text.c_str());
        }
    });

    // drawPolyline(points, color, closed?, thickness?)
    // points is a flat Lua table {x1, y1, x2, y2, ...}
    lua.set_function("drawPolyline", [&dc](
        sol::table pointsTable, const std::string& color,
        sol::optional<bool> closed, sol::optional<float> thickness
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        bool isClosed = closed.value_or(false);
        float t = thickness.value_or(1.0f);
        float ox = dc.offset.x, oy = dc.offset.y;

        int len = static_cast<int>(pointsTable.size());
        int numPoints = len / 2;
        std::vector<ImVec2> points(numPoints);
        std::vector<float> recordedFloats;

        for (int i = 0; i < numPoints; i++) {
            float px = pointsTable[i * 2 + 1].get<float>();
            float py = pointsTable[i * 2 + 2].get<float>();
            points[i] = {px + ox, py + oy};
            if (dc.recording) {
                recordedFloats.push_back(px + ox);
                recordedFloats.push_back(py + oy);
            }
        }

        if (dc.recording) {
            recordedFloats.push_back(isClosed ? 1.0f : 0.0f);
            recordedFloats.push_back(t);
            dc.recorded.push_back({"drawPolyline", recordedFloats, col, ""});
        }
        if (dc.drawList && numPoints > 0) {
            dc.drawList->AddPolyline(
                points.data(), numPoints, col,
                isClosed ? ImDrawFlags_Closed : ImDrawFlags_None, t);
        }
    });

    // drawBezierCubic(x1,y1, x2,y2, x3,y3, x4,y4, color, thickness?)
    lua.set_function("drawBezierCubic", [&dc](
        float x1, float y1, float x2, float y2,
        float x3, float y3, float x4, float y4,
        const std::string& color, sol::optional<float> thickness
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float t = thickness.value_or(1.0f);
        float ox = dc.offset.x, oy = dc.offset.y;
        if (dc.recording) {
            dc.recorded.push_back({"drawBezierCubic",
                {x1 + ox, y1 + oy, x2 + ox, y2 + oy,
                 x3 + ox, y3 + oy, x4 + ox, y4 + oy, t}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddBezierCubic(
                {x1 + ox, y1 + oy}, {x2 + ox, y2 + oy},
                {x3 + ox, y3 + oy}, {x4 + ox, y4 + oy}, col, t);
        }
    });

    // drawNgon(cx, cy, radius, color, numSegments, thickness?)
    lua.set_function("drawNgon", [&dc](
        float cx, float cy, float radius,
        const std::string& color, int numSegments, sol::optional<float> thickness
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float t = thickness.value_or(1.0f);
        if (dc.recording) {
            dc.recorded.push_back({"drawNgon",
                {cx + dc.offset.x, cy + dc.offset.y, radius, (float)numSegments, t}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddNgon({cx + dc.offset.x, cy + dc.offset.y}, radius, col, numSegments, t);
        }
    });

    // drawNgonFilled(cx, cy, radius, color, numSegments)
    lua.set_function("drawNgonFilled", [&dc](
        float cx, float cy, float radius,
        const std::string& color, int numSegments
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        if (dc.recording) {
            dc.recorded.push_back({"drawNgonFilled",
                {cx + dc.offset.x, cy + dc.offset.y, radius, (float)numSegments}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddNgonFilled({cx + dc.offset.x, cy + dc.offset.y}, radius, col, numSegments);
        }
    });

    // drawEllipse(cx, cy, rx, ry, color, thickness?, rotation?)
    lua.set_function("drawEllipse", [&dc](
        float cx, float cy, float rx, float ry,
        const std::string& color, sol::optional<float> thickness, sol::optional<float> rotation
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float t = thickness.value_or(1.0f);
        float rot = rotation.value_or(0.0f);
        if (dc.recording) {
            dc.recorded.push_back({"drawEllipse",
                {cx + dc.offset.x, cy + dc.offset.y, rx, ry, t, rot}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddEllipse({cx + dc.offset.x, cy + dc.offset.y}, {rx, ry}, col, rot, 0, t);
        }
    });

    // drawEllipseFilled(cx, cy, rx, ry, color, rotation?)
    lua.set_function("drawEllipseFilled", [&dc](
        float cx, float cy, float rx, float ry,
        const std::string& color, sol::optional<float> rotation
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float rot = rotation.value_or(0.0f);
        if (dc.recording) {
            dc.recorded.push_back({"drawEllipseFilled",
                {cx + dc.offset.x, cy + dc.offset.y, rx, ry, rot}, col, ""});
        }
        if (dc.drawList) {
            dc.drawList->AddEllipseFilled({cx + dc.offset.x, cy + dc.offset.y}, {rx, ry}, col, rot);
        }
    });

    // drawImage(textureId, x, y, w, h, uvX0?, uvY0?, uvX1?, uvY1?)
    lua.set_function("drawImage", [&dc](
        const std::string& textureId, float x, float y, float w, float h,
        sol::optional<float> uvX0, sol::optional<float> uvY0,
        sol::optional<float> uvX1, sol::optional<float> uvY1
    ) {
        float uv0x = uvX0.value_or(0.0f);
        float uv0y = uvY0.value_or(0.0f);
        float uv1x = uvX1.value_or(1.0f);
        float uv1y = uvY1.value_or(1.0f);
        float ox = dc.offset.x, oy = dc.offset.y;
        if (dc.recording) {
            dc.recorded.push_back({"drawImage",
                {x + ox, y + oy, w, h, uv0x, uv0y, uv1x, uv1y}, 0, textureId});
        }
        if (dc.drawList && dc.textureLookup) {
            ImTextureID texId = dc.textureLookup(textureId);
            if (texId) {
                dc.drawList->AddImage(texId,
                    {x + ox, y + oy}, {x + w + ox, y + h + oy},
                    {uv0x, uv0y}, {uv1x, uv1y});
            }
        }
    });

    // drawConvexPolyFilled(points, color)
    // points is a flat Lua table {x1, y1, x2, y2, ...}
    lua.set_function("drawConvexPolyFilled", [&dc](
        sol::table pointsTable, const std::string& color
    ) {
        ImU32 col = parseCSSColor(color.c_str(), &dc);
        float ox = dc.offset.x, oy = dc.offset.y;

        int len = static_cast<int>(pointsTable.size());
        int numPoints = len / 2;
        std::vector<ImVec2> points(numPoints);
        std::vector<float> recordedFloats;

        for (int i = 0; i < numPoints; i++) {
            float px = pointsTable[i * 2 + 1].get<float>();
            float py = pointsTable[i * 2 + 2].get<float>();
            points[i] = {px + ox, py + oy};
            if (dc.recording) {
                recordedFloats.push_back(px + ox);
                recordedFloats.push_back(py + oy);
            }
        }

        if (dc.recording) {
            dc.recorded.push_back({"drawConvexPolyFilled", recordedFloats, col, ""});
        }
        if (dc.drawList && numPoints > 0) {
            dc.drawList->AddConvexPolyFilled(points.data(), numPoints, col);
        }
    });

    // measureText(text) — returns {width, height}
    lua.set_function("measureText", [&dc, &lua](const std::string& text) -> sol::table {
        float w = 0, h = 0;
        if (dc.currentFont) {
            ImVec2 size = dc.currentFont->CalcTextSizeA(dc.currentFont->LegacySize, FLT_MAX, 0.0f, text.c_str());
            w = size.x;
            h = size.y;
        } else if (dc.recording) {
            w = 7.0f * (float)text.size();
            h = 16.0f;
        }

        if (dc.recording) {
            dc.recorded.push_back({"measureText", {w, h}, 0, text});
        }

        sol::table result = lua.create_table();
        result["width"] = w;
        result["height"] = h;
        return result;
    });

    // pushClipRect(x1, y1, x2, y2)
    lua.set_function("pushClipRect", [&dc](float x1, float y1, float x2, float y2) {
        float ox = dc.offset.x, oy = dc.offset.y;
        if (dc.recording) {
            dc.recorded.push_back({"pushClipRect",
                {x1 + ox, y1 + oy, x2 + ox, y2 + oy}, 0, ""});
        }
        if (dc.drawList) {
            dc.drawList->PushClipRect({x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, true);
        }
    });

    // popClipRect()
    lua.set_function("popClipRect", [&dc]() {
        if (dc.recording) {
            dc.recorded.push_back({"popClipRect", {}, 0, ""});
        }
        if (dc.drawList) {
            dc.drawList->PopClipRect();
        }
    });
}

} // namespace Sol2DrawBindings
