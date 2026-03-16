#pragma once

extern "C" {
#include <quickjs.h>
}

#include "draw_context.h"

namespace QuickJSDrawBindings {

inline DrawContext* getDC(JSContext* ctx) {
    return static_cast<DrawContext*>(JS_GetContextOpaque(ctx));
}

inline double getFloat(JSContext* ctx, int argc, JSValue* argv, int i, double def = 0.0) {
    if (i >= argc) return def;
    double v;
    return JS_ToFloat64(ctx, &v, argv[i]) == 0 ? v : def;
}

inline int32_t getInt(JSContext* ctx, int argc, JSValue* argv, int i, int32_t def = 0) {
    if (i >= argc) return def;
    int32_t v;
    return JS_ToInt32(ctx, &v, argv[i]) == 0 ? v : def;
}

// --- Draw functions ---

// drawLine(x1, y1, x2, y2, color, thickness?)
inline JSValue js_drawLine(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 5) return JS_UNDEFINED;

    float x1 = (float)getFloat(ctx, argc, argv, 0);
    float y1 = (float)getFloat(ctx, argc, argv, 1);
    float x2 = (float)getFloat(ctx, argc, argv, 2);
    float y2 = (float)getFloat(ctx, argc, argv, 3);
    const char* cs = JS_ToCString(ctx, argv[4]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    float thickness = (float)getFloat(ctx, argc, argv, 5, 1.0);

    if (dc->recording) {
        dc->recorded.push_back({"drawLine",
            {x1 + dc->offset.x, y1 + dc->offset.y,
             x2 + dc->offset.x, y2 + dc->offset.y, thickness},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddLine(
            {x1 + dc->offset.x, y1 + dc->offset.y},
            {x2 + dc->offset.x, y2 + dc->offset.y},
            color, thickness);
    }
    return JS_UNDEFINED;
}

// drawRect(x, y, w, h, color, thickness?, rounding?)
inline JSValue js_drawRect(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 5) return JS_UNDEFINED;

    float x = (float)getFloat(ctx, argc, argv, 0);
    float y = (float)getFloat(ctx, argc, argv, 1);
    float w = (float)getFloat(ctx, argc, argv, 2);
    float h = (float)getFloat(ctx, argc, argv, 3);
    const char* cs = JS_ToCString(ctx, argv[4]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    float thickness = (float)getFloat(ctx, argc, argv, 5, 1.0);
    float rounding = (float)getFloat(ctx, argc, argv, 6, 0.0);

    float ox = dc->offset.x, oy = dc->offset.y;
    if (dc->recording) {
        dc->recorded.push_back({"drawRect",
            {x + ox, y + oy, w, h, thickness, rounding}, color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddRect(
            {x + ox, y + oy}, {x + w + ox, y + h + oy},
            color, rounding, 0, thickness);
    }
    return JS_UNDEFINED;
}

// drawRectFilled(x, y, w, h, color, rounding?)
inline JSValue js_drawRectFilled(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 5) return JS_UNDEFINED;

    float x = (float)getFloat(ctx, argc, argv, 0);
    float y = (float)getFloat(ctx, argc, argv, 1);
    float w = (float)getFloat(ctx, argc, argv, 2);
    float h = (float)getFloat(ctx, argc, argv, 3);
    const char* cs = JS_ToCString(ctx, argv[4]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    float rounding = (float)getFloat(ctx, argc, argv, 5, 0.0);

    float ox = dc->offset.x, oy = dc->offset.y;
    if (dc->recording) {
        dc->recorded.push_back({"drawRectFilled",
            {x + ox, y + oy, w, h, rounding}, color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddRectFilled(
            {x + ox, y + oy}, {x + w + ox, y + h + oy},
            color, rounding);
    }
    return JS_UNDEFINED;
}

// drawCircle(cx, cy, radius, color, thickness?, segments?)
inline JSValue js_drawCircle(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 4) return JS_UNDEFINED;

    float cx = (float)getFloat(ctx, argc, argv, 0);
    float cy = (float)getFloat(ctx, argc, argv, 1);
    float radius = (float)getFloat(ctx, argc, argv, 2);
    const char* cs = JS_ToCString(ctx, argv[3]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    float thickness = (float)getFloat(ctx, argc, argv, 4, 1.0);
    int segments = getInt(ctx, argc, argv, 5, 0);

    if (dc->recording) {
        dc->recorded.push_back({"drawCircle",
            {cx + dc->offset.x, cy + dc->offset.y, radius, thickness, (float)segments},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddCircle(
            {cx + dc->offset.x, cy + dc->offset.y},
            radius, color, segments, thickness);
    }
    return JS_UNDEFINED;
}

// drawCircleFilled(cx, cy, radius, color, segments?)
inline JSValue js_drawCircleFilled(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 4) return JS_UNDEFINED;

    float cx = (float)getFloat(ctx, argc, argv, 0);
    float cy = (float)getFloat(ctx, argc, argv, 1);
    float radius = (float)getFloat(ctx, argc, argv, 2);
    const char* cs = JS_ToCString(ctx, argv[3]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    int segments = getInt(ctx, argc, argv, 4, 0);

    if (dc->recording) {
        dc->recorded.push_back({"drawCircleFilled",
            {cx + dc->offset.x, cy + dc->offset.y, radius, (float)segments},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddCircleFilled(
            {cx + dc->offset.x, cy + dc->offset.y},
            radius, color, segments);
    }
    return JS_UNDEFINED;
}

// drawTriangle(x1, y1, x2, y2, x3, y3, color, thickness?)
inline JSValue js_drawTriangle(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 7) return JS_UNDEFINED;

    float x1 = (float)getFloat(ctx, argc, argv, 0);
    float y1 = (float)getFloat(ctx, argc, argv, 1);
    float x2 = (float)getFloat(ctx, argc, argv, 2);
    float y2 = (float)getFloat(ctx, argc, argv, 3);
    float x3 = (float)getFloat(ctx, argc, argv, 4);
    float y3 = (float)getFloat(ctx, argc, argv, 5);
    const char* cs = JS_ToCString(ctx, argv[6]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    float thickness = (float)getFloat(ctx, argc, argv, 7, 1.0);

    float ox = dc->offset.x, oy = dc->offset.y;
    if (dc->recording) {
        dc->recorded.push_back({"drawTriangle",
            {x1 + ox, y1 + oy, x2 + ox, y2 + oy, x3 + ox, y3 + oy, thickness},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddTriangle(
            {x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, {x3 + ox, y3 + oy},
            color, thickness);
    }
    return JS_UNDEFINED;
}

// drawTriangleFilled(x1, y1, x2, y2, x3, y3, color)
inline JSValue js_drawTriangleFilled(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 7) return JS_UNDEFINED;

    float x1 = (float)getFloat(ctx, argc, argv, 0);
    float y1 = (float)getFloat(ctx, argc, argv, 1);
    float x2 = (float)getFloat(ctx, argc, argv, 2);
    float y2 = (float)getFloat(ctx, argc, argv, 3);
    float x3 = (float)getFloat(ctx, argc, argv, 4);
    float y3 = (float)getFloat(ctx, argc, argv, 5);
    const char* cs = JS_ToCString(ctx, argv[6]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);

    float ox = dc->offset.x, oy = dc->offset.y;
    if (dc->recording) {
        dc->recorded.push_back({"drawTriangleFilled",
            {x1 + ox, y1 + oy, x2 + ox, y2 + oy, x3 + ox, y3 + oy},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddTriangleFilled(
            {x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, {x3 + ox, y3 + oy},
            color);
    }
    return JS_UNDEFINED;
}

// drawText(x, y, color, text)
inline JSValue js_drawText(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 4) return JS_UNDEFINED;

    float x = (float)getFloat(ctx, argc, argv, 0);
    float y = (float)getFloat(ctx, argc, argv, 1);
    const char* cs = JS_ToCString(ctx, argv[2]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    const char* text = JS_ToCString(ctx, argv[3]);

    if (dc->recording) {
        dc->recorded.push_back({"drawText",
            {x + dc->offset.x, y + dc->offset.y},
            color, text ? text : ""});
    }
    if (dc->drawList) {
        dc->drawList->AddText(
            {x + dc->offset.x, y + dc->offset.y},
            color, text);
    }
    if (text) JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

// drawPolyline(points, color, closed?, thickness?)
// points is a flat JS array [x1, y1, x2, y2, ...]
inline JSValue js_drawPolyline(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 2) return JS_UNDEFINED;

    JSValue pointsArr = argv[0];
    JSValue lengthVal = JS_GetPropertyStr(ctx, pointsArr, "length");
    int32_t len = 0;
    JS_ToInt32(ctx, &len, lengthVal);
    JS_FreeValue(ctx, lengthVal);

    int numPoints = len / 2;
    std::vector<ImVec2> points(numPoints);
    std::vector<float> recordedFloats;

    float ox = dc->offset.x, oy = dc->offset.y;
    for (int i = 0; i < numPoints; i++) {
        JSValue xv = JS_GetPropertyUint32(ctx, pointsArr, i * 2);
        JSValue yv = JS_GetPropertyUint32(ctx, pointsArr, i * 2 + 1);
        double px, py;
        JS_ToFloat64(ctx, &px, xv);
        JS_ToFloat64(ctx, &py, yv);
        JS_FreeValue(ctx, xv);
        JS_FreeValue(ctx, yv);
        points[i] = {(float)px + ox, (float)py + oy};
        if (dc->recording) {
            recordedFloats.push_back((float)px + ox);
            recordedFloats.push_back((float)py + oy);
        }
    }

    const char* cs = JS_ToCString(ctx, argv[1]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);

    int closed = (argc > 2) ? JS_ToBool(ctx, argv[2]) : 0;
    float thickness = (float)getFloat(ctx, argc, argv, 3, 1.0);

    if (dc->recording) {
        recordedFloats.push_back(closed ? 1.0f : 0.0f);
        recordedFloats.push_back(thickness);
        dc->recorded.push_back({"drawPolyline", recordedFloats, color, ""});
    }
    if (dc->drawList && numPoints > 0) {
        dc->drawList->AddPolyline(
            points.data(), numPoints, color,
            closed ? ImDrawFlags_Closed : ImDrawFlags_None,
            thickness);
    }
    return JS_UNDEFINED;
}

// drawBezierCubic(x1,y1, x2,y2, x3,y3, x4,y4, color, thickness?)
inline JSValue js_drawBezierCubic(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 9) return JS_UNDEFINED;

    float x1 = (float)getFloat(ctx, argc, argv, 0);
    float y1 = (float)getFloat(ctx, argc, argv, 1);
    float x2 = (float)getFloat(ctx, argc, argv, 2);
    float y2 = (float)getFloat(ctx, argc, argv, 3);
    float x3 = (float)getFloat(ctx, argc, argv, 4);
    float y3 = (float)getFloat(ctx, argc, argv, 5);
    float x4 = (float)getFloat(ctx, argc, argv, 6);
    float y4 = (float)getFloat(ctx, argc, argv, 7);
    const char* cs = JS_ToCString(ctx, argv[8]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    float thickness = (float)getFloat(ctx, argc, argv, 9, 1.0);

    float ox = dc->offset.x, oy = dc->offset.y;
    if (dc->recording) {
        dc->recorded.push_back({"drawBezierCubic",
            {x1 + ox, y1 + oy, x2 + ox, y2 + oy,
             x3 + ox, y3 + oy, x4 + ox, y4 + oy, thickness},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddBezierCubic(
            {x1 + ox, y1 + oy}, {x2 + ox, y2 + oy},
            {x3 + ox, y3 + oy}, {x4 + ox, y4 + oy},
            color, thickness);
    }
    return JS_UNDEFINED;
}

// drawNgon(cx, cy, radius, color, numSegments, thickness?)
inline JSValue js_drawNgon(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 5) return JS_UNDEFINED;

    float cx = (float)getFloat(ctx, argc, argv, 0);
    float cy = (float)getFloat(ctx, argc, argv, 1);
    float radius = (float)getFloat(ctx, argc, argv, 2);
    const char* cs = JS_ToCString(ctx, argv[3]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    int numSegments = getInt(ctx, argc, argv, 4);
    float thickness = (float)getFloat(ctx, argc, argv, 5, 1.0);

    if (dc->recording) {
        dc->recorded.push_back({"drawNgon",
            {cx + dc->offset.x, cy + dc->offset.y, radius, (float)numSegments, thickness},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddNgon(
            {cx + dc->offset.x, cy + dc->offset.y},
            radius, color, numSegments, thickness);
    }
    return JS_UNDEFINED;
}

// drawNgonFilled(cx, cy, radius, color, numSegments)
inline JSValue js_drawNgonFilled(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 5) return JS_UNDEFINED;

    float cx = (float)getFloat(ctx, argc, argv, 0);
    float cy = (float)getFloat(ctx, argc, argv, 1);
    float radius = (float)getFloat(ctx, argc, argv, 2);
    const char* cs = JS_ToCString(ctx, argv[3]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    int numSegments = getInt(ctx, argc, argv, 4);

    if (dc->recording) {
        dc->recorded.push_back({"drawNgonFilled",
            {cx + dc->offset.x, cy + dc->offset.y, radius, (float)numSegments},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddNgonFilled(
            {cx + dc->offset.x, cy + dc->offset.y},
            radius, color, numSegments);
    }
    return JS_UNDEFINED;
}

// drawEllipse(cx, cy, rx, ry, color, thickness?, rotation?)
inline JSValue js_drawEllipse(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 5) return JS_UNDEFINED;

    float cx = (float)getFloat(ctx, argc, argv, 0);
    float cy = (float)getFloat(ctx, argc, argv, 1);
    float rx = (float)getFloat(ctx, argc, argv, 2);
    float ry = (float)getFloat(ctx, argc, argv, 3);
    const char* cs = JS_ToCString(ctx, argv[4]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    float thickness = (float)getFloat(ctx, argc, argv, 5, 1.0);
    float rotation = (float)getFloat(ctx, argc, argv, 6, 0.0);

    if (dc->recording) {
        dc->recorded.push_back({"drawEllipse",
            {cx + dc->offset.x, cy + dc->offset.y, rx, ry, thickness, rotation},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddEllipse(
            {cx + dc->offset.x, cy + dc->offset.y},
            {rx, ry}, color, rotation, 0, thickness);
    }
    return JS_UNDEFINED;
}

// drawEllipseFilled(cx, cy, rx, ry, color, rotation?)
inline JSValue js_drawEllipseFilled(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 5) return JS_UNDEFINED;

    float cx = (float)getFloat(ctx, argc, argv, 0);
    float cy = (float)getFloat(ctx, argc, argv, 1);
    float rx = (float)getFloat(ctx, argc, argv, 2);
    float ry = (float)getFloat(ctx, argc, argv, 3);
    const char* cs = JS_ToCString(ctx, argv[4]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);
    float rotation = (float)getFloat(ctx, argc, argv, 5, 0.0);

    if (dc->recording) {
        dc->recorded.push_back({"drawEllipseFilled",
            {cx + dc->offset.x, cy + dc->offset.y, rx, ry, rotation},
            color, ""});
    }
    if (dc->drawList) {
        dc->drawList->AddEllipseFilled(
            {cx + dc->offset.x, cy + dc->offset.y},
            {rx, ry}, color, rotation);
    }
    return JS_UNDEFINED;
}

// drawImage(textureId, x, y, w, h, uvX0?, uvY0?, uvX1?, uvY1?)
inline JSValue js_drawImage(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 5) return JS_UNDEFINED;

    const char* textureId = JS_ToCString(ctx, argv[0]);
    float x = (float)getFloat(ctx, argc, argv, 1);
    float y = (float)getFloat(ctx, argc, argv, 2);
    float w = (float)getFloat(ctx, argc, argv, 3);
    float h = (float)getFloat(ctx, argc, argv, 4);
    float uvX0 = (float)getFloat(ctx, argc, argv, 5, 0.0);
    float uvY0 = (float)getFloat(ctx, argc, argv, 6, 0.0);
    float uvX1 = (float)getFloat(ctx, argc, argv, 7, 1.0);
    float uvY1 = (float)getFloat(ctx, argc, argv, 8, 1.0);

    float ox = dc->offset.x, oy = dc->offset.y;
    if (dc->recording) {
        dc->recorded.push_back({"drawImage",
            {x + ox, y + oy, w, h, uvX0, uvY0, uvX1, uvY1},
            0, textureId ? textureId : ""});
    }
    if (dc->drawList && dc->textureLookup) {
        std::string id = textureId ? textureId : "";
        ImTextureID texId = dc->textureLookup(id);
        if (texId) {
            dc->drawList->AddImage(texId,
                {x + ox, y + oy}, {x + w + ox, y + h + oy},
                {uvX0, uvY0}, {uvX1, uvY1});
        }
    }
    if (textureId) JS_FreeCString(ctx, textureId);
    return JS_UNDEFINED;
}

// drawConvexPolyFilled(points, color)
// points is a flat JS array [x1, y1, x2, y2, ...]
inline JSValue js_drawConvexPolyFilled(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 2) return JS_UNDEFINED;

    JSValue pointsArr = argv[0];
    JSValue lengthVal = JS_GetPropertyStr(ctx, pointsArr, "length");
    int32_t len = 0;
    JS_ToInt32(ctx, &len, lengthVal);
    JS_FreeValue(ctx, lengthVal);

    int numPoints = len / 2;
    std::vector<ImVec2> points(numPoints);
    std::vector<float> recordedFloats;

    float ox = dc->offset.x, oy = dc->offset.y;
    for (int i = 0; i < numPoints; i++) {
        JSValue xv = JS_GetPropertyUint32(ctx, pointsArr, i * 2);
        JSValue yv = JS_GetPropertyUint32(ctx, pointsArr, i * 2 + 1);
        double px, py;
        JS_ToFloat64(ctx, &px, xv);
        JS_ToFloat64(ctx, &py, yv);
        JS_FreeValue(ctx, xv);
        JS_FreeValue(ctx, yv);
        points[i] = {(float)px + ox, (float)py + oy};
        if (dc->recording) {
            recordedFloats.push_back((float)px + ox);
            recordedFloats.push_back((float)py + oy);
        }
    }

    const char* cs = JS_ToCString(ctx, argv[1]);
    ImU32 color = parseCSSColor(cs, dc);
    JS_FreeCString(ctx, cs);

    if (dc->recording) {
        dc->recorded.push_back({"drawConvexPolyFilled", recordedFloats, color, ""});
    }
    if (dc->drawList && numPoints > 0) {
        dc->drawList->AddConvexPolyFilled(points.data(), numPoints, color);
    }
    return JS_UNDEFINED;
}

// __measureText(text) — returns {width, height} using ImFont::CalcTextSizeA
inline JSValue js_measureText(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 1) return JS_UNDEFINED;

    const char* text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_UNDEFINED;

    float w = 0, h = 0;
    if (dc->currentFont) {
        ImVec2 size = dc->currentFont->CalcTextSizeA(dc->currentFont->LegacySize, FLT_MAX, 0.0f, text);
        w = size.x;
        h = size.y;
    } else if (dc->recording) {
        // In test mode, return a predictable size for verification
        w = 7.0f * (float)strlen(text); // ~7px per char approximation
        h = 16.0f;
    }

    if (dc->recording) {
        dc->recorded.push_back({"__measureText", {w, h}, 0, text ? text : ""});
    }

    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "width", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, result, "height", JS_NewFloat64(ctx, h));
    JS_FreeCString(ctx, text);
    return result;
}

// __pushClipRect(x1, y1, x2, y2) — calls ImDrawList::PushClipRect
inline JSValue js_pushClipRect(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc || argc < 4) return JS_UNDEFINED;

    float x1 = (float)getFloat(ctx, argc, argv, 0);
    float y1 = (float)getFloat(ctx, argc, argv, 1);
    float x2 = (float)getFloat(ctx, argc, argv, 2);
    float y2 = (float)getFloat(ctx, argc, argv, 3);

    float ox = dc->offset.x, oy = dc->offset.y;
    if (dc->recording) {
        dc->recorded.push_back({"__pushClipRect",
            {x1 + ox, y1 + oy, x2 + ox, y2 + oy}, 0, ""});
    }
    if (dc->drawList) {
        dc->drawList->PushClipRect({x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, true);
    }
    return JS_UNDEFINED;
}

// __popClipRect() — calls ImDrawList::PopClipRect
inline JSValue js_popClipRect(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
    auto* dc = getDC(ctx);
    if (!dc) return JS_UNDEFINED;

    if (dc->recording) {
        dc->recorded.push_back({"__popClipRect", {}, 0, ""});
    }
    if (dc->drawList) {
        dc->drawList->PopClipRect();
    }
    return JS_UNDEFINED;
}

// Register all draw functions on the JS global object
inline void registerDrawBindings(JSContext* ctx) {
    JSValue global = JS_GetGlobalObject(ctx);

    auto reg = [&](const char* name, JSCFunction* func, int minArgs) {
        JS_SetPropertyStr(ctx, global, name,
            JS_NewCFunction(ctx, func, name, minArgs));
    };

    reg("drawLine",           js_drawLine,           5);
    reg("drawRect",           js_drawRect,           5);
    reg("drawRectFilled",     js_drawRectFilled,     5);
    reg("drawCircle",         js_drawCircle,         4);
    reg("drawCircleFilled",   js_drawCircleFilled,   4);
    reg("drawTriangle",       js_drawTriangle,       7);
    reg("drawTriangleFilled", js_drawTriangleFilled, 7);
    reg("drawText",           js_drawText,           4);
    reg("drawPolyline",       js_drawPolyline,       2);
    reg("drawBezierCubic",    js_drawBezierCubic,    9);
    reg("drawNgon",           js_drawNgon,           5);
    reg("drawNgonFilled",     js_drawNgonFilled,     5);
    reg("drawEllipse",        js_drawEllipse,        5);
    reg("drawEllipseFilled",  js_drawEllipseFilled,  5);
    reg("drawImage",              js_drawImage,          5);
    reg("drawConvexPolyFilled",   js_drawConvexPolyFilled, 2);
    reg("__measureText",          js_measureText,        1);
    reg("__pushClipRect",         js_pushClipRect,       4);
    reg("__popClipRect",          js_popClipRect,        0);

    JS_FreeValue(ctx, global);
}

} // namespace QuickJSDrawBindings
