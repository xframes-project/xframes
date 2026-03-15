#pragma once

extern "C" {
#include <janet.h>
}

#include "draw_context.h"

namespace JanetDrawBindings {

// File-scope DrawContext pointer — set before registration, used by all cfuns.
// Safe: one Janet VM per widget, all draw calls on the render thread.
static DrawContext* s_dc = nullptr;

static Janet cfun_draw_line(int32_t argc, Janet *argv) {
    janet_arity(argc, 5, 6);
    float x1 = (float)janet_getnumber(argv, 0);
    float y1 = (float)janet_getnumber(argv, 1);
    float x2 = (float)janet_getnumber(argv, 2);
    float y2 = (float)janet_getnumber(argv, 3);
    const char* color = (const char*)janet_getstring(argv, 4);
    float thickness = (float)janet_optnumber(argv, argc, 5, 1.0);

    ImU32 col = parseCSSColor(color);
    float ox = s_dc->offset.x, oy = s_dc->offset.y;
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawLine",
            {x1 + ox, y1 + oy, x2 + ox, y2 + oy, thickness}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddLine({x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, col, thickness);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_rect(int32_t argc, Janet *argv) {
    janet_arity(argc, 5, 7);
    float x = (float)janet_getnumber(argv, 0);
    float y = (float)janet_getnumber(argv, 1);
    float w = (float)janet_getnumber(argv, 2);
    float h = (float)janet_getnumber(argv, 3);
    const char* color = (const char*)janet_getstring(argv, 4);
    float thickness = (float)janet_optnumber(argv, argc, 5, 1.0);
    float rounding = (float)janet_optnumber(argv, argc, 6, 0.0);

    ImU32 col = parseCSSColor(color);
    float ox = s_dc->offset.x, oy = s_dc->offset.y;
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawRect",
            {x + ox, y + oy, w, h, thickness, rounding}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddRect({x + ox, y + oy}, {x + w + ox, y + h + oy}, col, rounding, 0, thickness);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_rect_filled(int32_t argc, Janet *argv) {
    janet_arity(argc, 5, 6);
    float x = (float)janet_getnumber(argv, 0);
    float y = (float)janet_getnumber(argv, 1);
    float w = (float)janet_getnumber(argv, 2);
    float h = (float)janet_getnumber(argv, 3);
    const char* color = (const char*)janet_getstring(argv, 4);
    float rounding = (float)janet_optnumber(argv, argc, 5, 0.0);

    ImU32 col = parseCSSColor(color);
    float ox = s_dc->offset.x, oy = s_dc->offset.y;
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawRectFilled",
            {x + ox, y + oy, w, h, rounding}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddRectFilled({x + ox, y + oy}, {x + w + ox, y + h + oy}, col, rounding);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_circle(int32_t argc, Janet *argv) {
    janet_arity(argc, 4, 6);
    float cx = (float)janet_getnumber(argv, 0);
    float cy = (float)janet_getnumber(argv, 1);
    float radius = (float)janet_getnumber(argv, 2);
    const char* color = (const char*)janet_getstring(argv, 3);
    float thickness = (float)janet_optnumber(argv, argc, 4, 1.0);
    int segments = janet_optinteger(argv, argc, 5, 0);

    ImU32 col = parseCSSColor(color);
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawCircle",
            {cx + s_dc->offset.x, cy + s_dc->offset.y, radius, thickness, (float)segments}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddCircle({cx + s_dc->offset.x, cy + s_dc->offset.y}, radius, col, segments, thickness);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_circle_filled(int32_t argc, Janet *argv) {
    janet_arity(argc, 4, 5);
    float cx = (float)janet_getnumber(argv, 0);
    float cy = (float)janet_getnumber(argv, 1);
    float radius = (float)janet_getnumber(argv, 2);
    const char* color = (const char*)janet_getstring(argv, 3);
    int segments = janet_optinteger(argv, argc, 4, 0);

    ImU32 col = parseCSSColor(color);
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawCircleFilled",
            {cx + s_dc->offset.x, cy + s_dc->offset.y, radius, (float)segments}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddCircleFilled({cx + s_dc->offset.x, cy + s_dc->offset.y}, radius, col, segments);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_triangle(int32_t argc, Janet *argv) {
    janet_arity(argc, 7, 8);
    float x1 = (float)janet_getnumber(argv, 0);
    float y1 = (float)janet_getnumber(argv, 1);
    float x2 = (float)janet_getnumber(argv, 2);
    float y2 = (float)janet_getnumber(argv, 3);
    float x3 = (float)janet_getnumber(argv, 4);
    float y3 = (float)janet_getnumber(argv, 5);
    const char* color = (const char*)janet_getstring(argv, 6);
    float thickness = (float)janet_optnumber(argv, argc, 7, 1.0);

    ImU32 col = parseCSSColor(color);
    float ox = s_dc->offset.x, oy = s_dc->offset.y;
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawTriangle",
            {x1 + ox, y1 + oy, x2 + ox, y2 + oy, x3 + ox, y3 + oy, thickness}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddTriangle(
            {x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, {x3 + ox, y3 + oy}, col, thickness);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_triangle_filled(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 7);
    float x1 = (float)janet_getnumber(argv, 0);
    float y1 = (float)janet_getnumber(argv, 1);
    float x2 = (float)janet_getnumber(argv, 2);
    float y2 = (float)janet_getnumber(argv, 3);
    float x3 = (float)janet_getnumber(argv, 4);
    float y3 = (float)janet_getnumber(argv, 5);
    const char* color = (const char*)janet_getstring(argv, 6);

    ImU32 col = parseCSSColor(color);
    float ox = s_dc->offset.x, oy = s_dc->offset.y;
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawTriangleFilled",
            {x1 + ox, y1 + oy, x2 + ox, y2 + oy, x3 + ox, y3 + oy}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddTriangleFilled(
            {x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, {x3 + ox, y3 + oy}, col);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_text(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 4);
    float x = (float)janet_getnumber(argv, 0);
    float y = (float)janet_getnumber(argv, 1);
    const char* color = (const char*)janet_getstring(argv, 2);
    const char* text = (const char*)janet_getstring(argv, 3);

    ImU32 col = parseCSSColor(color);
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawText",
            {x + s_dc->offset.x, y + s_dc->offset.y}, col, text});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddText({x + s_dc->offset.x, y + s_dc->offset.y}, col, text);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_polyline(int32_t argc, Janet *argv) {
    janet_arity(argc, 2, 4);
    // argv[0] = tuple/array of flat coords [x1 y1 x2 y2 ...]
    int32_t len = janet_length(argv[0]);
    const char* color = (const char*)janet_getstring(argv, 1);
    int isClosed = janet_optboolean(argv, argc, 2, 0);
    float thickness = (float)janet_optnumber(argv, argc, 3, 1.0);

    ImU32 col = parseCSSColor(color);
    float ox = s_dc->offset.x, oy = s_dc->offset.y;

    int numPoints = len / 2;
    std::vector<ImVec2> points(numPoints);
    std::vector<float> recordedFloats;

    for (int i = 0; i < numPoints; i++) {
        float px = (float)janet_unwrap_number(janet_getindex(argv[0], i * 2));
        float py = (float)janet_unwrap_number(janet_getindex(argv[0], i * 2 + 1));
        points[i] = {px + ox, py + oy};
        if (s_dc->recording) {
            recordedFloats.push_back(px + ox);
            recordedFloats.push_back(py + oy);
        }
    }

    if (s_dc->recording) {
        recordedFloats.push_back(isClosed ? 1.0f : 0.0f);
        recordedFloats.push_back(thickness);
        s_dc->recorded.push_back({"drawPolyline", recordedFloats, col, ""});
    }
    if (s_dc->drawList && numPoints > 0) {
        s_dc->drawList->AddPolyline(
            points.data(), numPoints, col,
            isClosed ? ImDrawFlags_Closed : ImDrawFlags_None, thickness);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_bezier_cubic(int32_t argc, Janet *argv) {
    janet_arity(argc, 9, 10);
    float x1 = (float)janet_getnumber(argv, 0);
    float y1 = (float)janet_getnumber(argv, 1);
    float x2 = (float)janet_getnumber(argv, 2);
    float y2 = (float)janet_getnumber(argv, 3);
    float x3 = (float)janet_getnumber(argv, 4);
    float y3 = (float)janet_getnumber(argv, 5);
    float x4 = (float)janet_getnumber(argv, 6);
    float y4 = (float)janet_getnumber(argv, 7);
    const char* color = (const char*)janet_getstring(argv, 8);
    float thickness = (float)janet_optnumber(argv, argc, 9, 1.0);

    ImU32 col = parseCSSColor(color);
    float ox = s_dc->offset.x, oy = s_dc->offset.y;
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawBezierCubic",
            {x1 + ox, y1 + oy, x2 + ox, y2 + oy,
             x3 + ox, y3 + oy, x4 + ox, y4 + oy, thickness}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddBezierCubic(
            {x1 + ox, y1 + oy}, {x2 + ox, y2 + oy},
            {x3 + ox, y3 + oy}, {x4 + ox, y4 + oy}, col, thickness);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_ngon(int32_t argc, Janet *argv) {
    janet_arity(argc, 5, 6);
    float cx = (float)janet_getnumber(argv, 0);
    float cy = (float)janet_getnumber(argv, 1);
    float radius = (float)janet_getnumber(argv, 2);
    const char* color = (const char*)janet_getstring(argv, 3);
    int numSegments = janet_getinteger(argv, 4);
    float thickness = (float)janet_optnumber(argv, argc, 5, 1.0);

    ImU32 col = parseCSSColor(color);
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawNgon",
            {cx + s_dc->offset.x, cy + s_dc->offset.y, radius, (float)numSegments, thickness}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddNgon({cx + s_dc->offset.x, cy + s_dc->offset.y}, radius, col, numSegments, thickness);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_ngon_filled(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 5);
    float cx = (float)janet_getnumber(argv, 0);
    float cy = (float)janet_getnumber(argv, 1);
    float radius = (float)janet_getnumber(argv, 2);
    const char* color = (const char*)janet_getstring(argv, 3);
    int numSegments = janet_getinteger(argv, 4);

    ImU32 col = parseCSSColor(color);
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawNgonFilled",
            {cx + s_dc->offset.x, cy + s_dc->offset.y, radius, (float)numSegments}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddNgonFilled({cx + s_dc->offset.x, cy + s_dc->offset.y}, radius, col, numSegments);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_ellipse(int32_t argc, Janet *argv) {
    janet_arity(argc, 5, 7);
    float cx = (float)janet_getnumber(argv, 0);
    float cy = (float)janet_getnumber(argv, 1);
    float rx = (float)janet_getnumber(argv, 2);
    float ry = (float)janet_getnumber(argv, 3);
    const char* color = (const char*)janet_getstring(argv, 4);
    float thickness = (float)janet_optnumber(argv, argc, 5, 1.0);
    float rotation = (float)janet_optnumber(argv, argc, 6, 0.0);

    ImU32 col = parseCSSColor(color);
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawEllipse",
            {cx + s_dc->offset.x, cy + s_dc->offset.y, rx, ry, thickness, rotation}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddEllipse({cx + s_dc->offset.x, cy + s_dc->offset.y}, {rx, ry}, col, rotation, 0, thickness);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_ellipse_filled(int32_t argc, Janet *argv) {
    janet_arity(argc, 5, 6);
    float cx = (float)janet_getnumber(argv, 0);
    float cy = (float)janet_getnumber(argv, 1);
    float rx = (float)janet_getnumber(argv, 2);
    float ry = (float)janet_getnumber(argv, 3);
    const char* color = (const char*)janet_getstring(argv, 4);
    float rotation = (float)janet_optnumber(argv, argc, 5, 0.0);

    ImU32 col = parseCSSColor(color);
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawEllipseFilled",
            {cx + s_dc->offset.x, cy + s_dc->offset.y, rx, ry, rotation}, col, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->AddEllipseFilled({cx + s_dc->offset.x, cy + s_dc->offset.y}, {rx, ry}, col, rotation);
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_image(int32_t argc, Janet *argv) {
    janet_arity(argc, 5, 9);
    const char* textureId = (const char*)janet_getstring(argv, 0);
    float x = (float)janet_getnumber(argv, 1);
    float y = (float)janet_getnumber(argv, 2);
    float w = (float)janet_getnumber(argv, 3);
    float h = (float)janet_getnumber(argv, 4);
    float uv0x = (float)janet_optnumber(argv, argc, 5, 0.0);
    float uv0y = (float)janet_optnumber(argv, argc, 6, 0.0);
    float uv1x = (float)janet_optnumber(argv, argc, 7, 1.0);
    float uv1y = (float)janet_optnumber(argv, argc, 8, 1.0);

    float ox = s_dc->offset.x, oy = s_dc->offset.y;
    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawImage",
            {x + ox, y + oy, w, h, uv0x, uv0y, uv1x, uv1y}, 0, textureId});
    }
    if (s_dc->drawList && s_dc->textureLookup) {
        ImTextureID texId = s_dc->textureLookup(textureId);
        if (texId) {
            s_dc->drawList->AddImage(texId,
                {x + ox, y + oy}, {x + w + ox, y + h + oy},
                {uv0x, uv0y}, {uv1x, uv1y});
        }
    }
    return janet_wrap_nil();
}

static Janet cfun_draw_convex_poly_filled(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 2);
    int32_t len = janet_length(argv[0]);
    const char* color = (const char*)janet_getstring(argv, 1);

    ImU32 col = parseCSSColor(color);
    float ox = s_dc->offset.x, oy = s_dc->offset.y;

    int numPoints = len / 2;
    std::vector<ImVec2> points(numPoints);
    std::vector<float> recordedFloats;

    for (int i = 0; i < numPoints; i++) {
        float px = (float)janet_unwrap_number(janet_getindex(argv[0], i * 2));
        float py = (float)janet_unwrap_number(janet_getindex(argv[0], i * 2 + 1));
        points[i] = {px + ox, py + oy};
        if (s_dc->recording) {
            recordedFloats.push_back(px + ox);
            recordedFloats.push_back(py + oy);
        }
    }

    if (s_dc->recording) {
        s_dc->recorded.push_back({"drawConvexPolyFilled", recordedFloats, col, ""});
    }
    if (s_dc->drawList && numPoints > 0) {
        s_dc->drawList->AddConvexPolyFilled(points.data(), numPoints, col);
    }
    return janet_wrap_nil();
}

static Janet cfun_measure_text(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 1);
    const char* text = (const char*)janet_getstring(argv, 0);

    float w = 0, h = 0;
    if (s_dc->currentFont) {
        ImVec2 size = s_dc->currentFont->CalcTextSizeA(s_dc->currentFont->LegacySize, FLT_MAX, 0.0f, text);
        w = size.x;
        h = size.y;
    } else if (s_dc->recording) {
        w = 7.0f * (float)strlen(text);
        h = 16.0f;
    }

    if (s_dc->recording) {
        s_dc->recorded.push_back({"measureText", {w, h}, 0, text});
    }

    // Return a Janet struct {:width w :height h}
    JanetKV* st = janet_struct_begin(2);
    janet_struct_put(st, janet_ckeywordv("width"), janet_wrap_number(w));
    janet_struct_put(st, janet_ckeywordv("height"), janet_wrap_number(h));
    return janet_wrap_struct(janet_struct_end(st));
}

static Janet cfun_push_clip_rect(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 4);
    float x1 = (float)janet_getnumber(argv, 0);
    float y1 = (float)janet_getnumber(argv, 1);
    float x2 = (float)janet_getnumber(argv, 2);
    float y2 = (float)janet_getnumber(argv, 3);

    float ox = s_dc->offset.x, oy = s_dc->offset.y;
    if (s_dc->recording) {
        s_dc->recorded.push_back({"pushClipRect",
            {x1 + ox, y1 + oy, x2 + ox, y2 + oy}, 0, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->PushClipRect({x1 + ox, y1 + oy}, {x2 + ox, y2 + oy}, true);
    }
    return janet_wrap_nil();
}

static Janet cfun_pop_clip_rect(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 0);
    (void)argv;
    if (s_dc->recording) {
        s_dc->recorded.push_back({"popClipRect", {}, 0, ""});
    }
    if (s_dc->drawList) {
        s_dc->drawList->PopClipRect();
    }
    return janet_wrap_nil();
}

static const JanetReg draw_cfuns[] = {
    {"draw-line", cfun_draw_line, "(draw-line x1 y1 x2 y2 color &opt thickness)"},
    {"draw-rect", cfun_draw_rect, "(draw-rect x y w h color &opt thickness rounding)"},
    {"draw-rect-filled", cfun_draw_rect_filled, "(draw-rect-filled x y w h color &opt rounding)"},
    {"draw-circle", cfun_draw_circle, "(draw-circle cx cy radius color &opt thickness segments)"},
    {"draw-circle-filled", cfun_draw_circle_filled, "(draw-circle-filled cx cy radius color &opt segments)"},
    {"draw-triangle", cfun_draw_triangle, "(draw-triangle x1 y1 x2 y2 x3 y3 color &opt thickness)"},
    {"draw-triangle-filled", cfun_draw_triangle_filled, "(draw-triangle-filled x1 y1 x2 y2 x3 y3 color)"},
    {"draw-text", cfun_draw_text, "(draw-text x y color text)"},
    {"draw-polyline", cfun_draw_polyline, "(draw-polyline points color &opt closed thickness)"},
    {"draw-bezier-cubic", cfun_draw_bezier_cubic, "(draw-bezier-cubic x1 y1 x2 y2 x3 y3 x4 y4 color &opt thickness)"},
    {"draw-ngon", cfun_draw_ngon, "(draw-ngon cx cy radius color num-segments &opt thickness)"},
    {"draw-ngon-filled", cfun_draw_ngon_filled, "(draw-ngon-filled cx cy radius color num-segments)"},
    {"draw-ellipse", cfun_draw_ellipse, "(draw-ellipse cx cy rx ry color &opt thickness rotation)"},
    {"draw-ellipse-filled", cfun_draw_ellipse_filled, "(draw-ellipse-filled cx cy rx ry color &opt rotation)"},
    {"draw-image", cfun_draw_image, "(draw-image texture-id x y w h &opt uv0x uv0y uv1x uv1y)"},
    {"draw-convex-poly-filled", cfun_draw_convex_poly_filled, "(draw-convex-poly-filled points color)"},
    {"measure-text", cfun_measure_text, "(measure-text text)"},
    {"push-clip-rect", cfun_push_clip_rect, "(push-clip-rect x1 y1 x2 y2)"},
    {"pop-clip-rect", cfun_pop_clip_rect, "(pop-clip-rect)"},
    {NULL, NULL, NULL}
};

inline void registerDrawBindings(JanetTable* env, DrawContext& dc) {
    s_dc = &dc;
    janet_cfuns(env, NULL, draw_cfuns);
}

} // namespace JanetDrawBindings
