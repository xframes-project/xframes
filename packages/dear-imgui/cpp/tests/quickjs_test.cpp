#include <gtest/gtest.h>

extern "C" {
#include <quickjs.h>
}

#include "quickjs_draw_bindings.h"

class QuickJSTest : public ::testing::Test {
protected:
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;

    void SetUp() override {
        rt = JS_NewRuntime();
        ASSERT_NE(rt, nullptr);
        ctx = JS_NewContext(rt);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) JS_FreeContext(ctx);
        if (rt) JS_FreeRuntime(rt);
    }
};

TEST_F(QuickJSTest, EvalSimpleExpression) {
    JSValue result = JS_Eval(ctx, "1 + 1", 5, "<test>", JS_EVAL_TYPE_GLOBAL);
    ASSERT_FALSE(JS_IsException(result));

    int32_t val;
    ASSERT_EQ(JS_ToInt32(ctx, &val, result), 0);
    EXPECT_EQ(val, 2);

    JS_FreeValue(ctx, result);
}

TEST_F(QuickJSTest, EvalArrowFunction) {
    const char* code = "const add = (a, b) => a + b; add(3, 4);";
    JSValue result = JS_Eval(ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    ASSERT_FALSE(JS_IsException(result));

    int32_t val;
    ASSERT_EQ(JS_ToInt32(ctx, &val, result), 0);
    EXPECT_EQ(val, 7);

    JS_FreeValue(ctx, result);
}

TEST_F(QuickJSTest, EvalDestructuring) {
    const char* code = "const { x, y } = { x: 10, y: 20 }; x + y;";
    JSValue result = JS_Eval(ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    ASSERT_FALSE(JS_IsException(result));

    int32_t val;
    ASSERT_EQ(JS_ToInt32(ctx, &val, result), 0);
    EXPECT_EQ(val, 30);

    JS_FreeValue(ctx, result);
}

TEST_F(QuickJSTest, ExposeCppFunctionToJS) {
    static int callCount = 0;
    callCount = 0;

    // Define a C function callable from JS
    auto nativeIncrement = [](JSContext* ctx, JSValue this_val,
                              int argc, JSValue* argv) -> JSValue {
        callCount++;
        return JS_NewInt32(ctx, callCount);
    };

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "increment",
        JS_NewCFunction(ctx, nativeIncrement, "increment", 0));
    JS_FreeValue(ctx, global);

    const char* code = "increment(); increment(); increment();";
    JSValue result = JS_Eval(ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    ASSERT_FALSE(JS_IsException(result));

    int32_t val;
    ASSERT_EQ(JS_ToInt32(ctx, &val, result), 0);
    EXPECT_EQ(val, 3);
    EXPECT_EQ(callCount, 3);

    JS_FreeValue(ctx, result);
}

// --- Draw binding tests ---

class QuickJSDrawTest : public ::testing::Test {
protected:
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;
    DrawContext dc;

    void SetUp() override {
        rt = JS_NewRuntime();
        ASSERT_NE(rt, nullptr);
        ctx = JS_NewContext(rt);
        ASSERT_NE(ctx, nullptr);

        dc.drawList = nullptr;
        dc.offset = {0, 0};
        dc.recording = true;
        dc.recorded.clear();

        JS_SetContextOpaque(ctx, &dc);
        QuickJSDrawBindings::registerDrawBindings(ctx);
    }

    void TearDown() override {
        if (ctx) JS_FreeContext(ctx);
        if (rt) JS_FreeRuntime(rt);
    }

    JSValue eval(const char* code) {
        return JS_Eval(ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    }
};

// Verify all 14 functions are registered and callable
TEST_F(QuickJSDrawTest, AllFunctionsRegistered) {
    const char* names[] = {
        "drawLine", "drawRect", "drawRectFilled",
        "drawCircle", "drawCircleFilled",
        "drawTriangle", "drawTriangleFilled",
        "drawText", "drawPolyline", "drawBezierCubic",
        "drawNgon", "drawNgonFilled",
        "drawEllipse", "drawEllipseFilled",
        "drawImage"
    };

    for (const char* name : names) {
        std::string code = std::string("typeof ") + name;
        JSValue result = eval(code.c_str());
        ASSERT_FALSE(JS_IsException(result)) << "Exception checking " << name;
        const char* typeStr = JS_ToCString(ctx, result);
        EXPECT_STREQ(typeStr, "function") << name << " should be a function";
        JS_FreeCString(ctx, typeStr);
        JS_FreeValue(ctx, result);
    }
}

// drawLine records correct parameters
TEST_F(QuickJSDrawTest, DrawLineBasic) {
    JSValue r = eval("drawLine(10, 20, 30, 40, '#ff0000')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawLine");
    EXPECT_FLOAT_EQ(call.floatArgs[0], 10.0f); // x1
    EXPECT_FLOAT_EQ(call.floatArgs[1], 20.0f); // y1
    EXPECT_FLOAT_EQ(call.floatArgs[2], 30.0f); // x2
    EXPECT_FLOAT_EQ(call.floatArgs[3], 40.0f); // y2
    EXPECT_FLOAT_EQ(call.floatArgs[4], 1.0f);  // default thickness
}

// drawLine with explicit thickness
TEST_F(QuickJSDrawTest, DrawLineWithThickness) {
    JSValue r = eval("drawLine(0, 0, 100, 100, '#000', 3.5)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 3.5f);
}

// Color parsing: #RRGGBB
TEST_F(QuickJSDrawTest, ColorParsingHex) {
    JSValue r = eval("drawCircleFilled(0, 0, 10, '#ff0000')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    ImU32 expected = IM_COL32(255, 0, 0, 255);
    EXPECT_EQ(dc.recorded[0].color, expected);
}

// Color parsing: named CSS color
TEST_F(QuickJSDrawTest, ColorParsingNamed) {
    JSValue r = eval("drawCircleFilled(0, 0, 10, 'blue')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    ImU32 expected = IM_COL32(0, 0, 255, 255);
    EXPECT_EQ(dc.recorded[0].color, expected);
}

// Offset is applied to coordinates
TEST_F(QuickJSDrawTest, OffsetApplied) {
    dc.offset = {100, 200};

    JSValue r = eval("drawLine(10, 20, 30, 40, '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& args = dc.recorded[0].floatArgs;
    EXPECT_FLOAT_EQ(args[0], 110.0f); // 10 + 100
    EXPECT_FLOAT_EQ(args[1], 220.0f); // 20 + 200
    EXPECT_FLOAT_EQ(args[2], 130.0f); // 30 + 100
    EXPECT_FLOAT_EQ(args[3], 240.0f); // 40 + 200
}

// drawRect records x, y, w, h, thickness, rounding
TEST_F(QuickJSDrawTest, DrawRect) {
    JSValue r = eval("drawRect(5, 10, 100, 50, '#00ff00', 2.0, 4.0)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawRect");
    EXPECT_FLOAT_EQ(call.floatArgs[0], 5.0f);   // x
    EXPECT_FLOAT_EQ(call.floatArgs[1], 10.0f);  // y
    EXPECT_FLOAT_EQ(call.floatArgs[2], 100.0f); // w
    EXPECT_FLOAT_EQ(call.floatArgs[3], 50.0f);  // h
    EXPECT_FLOAT_EQ(call.floatArgs[4], 2.0f);   // thickness
    EXPECT_FLOAT_EQ(call.floatArgs[5], 4.0f);   // rounding
}

// drawRectFilled
TEST_F(QuickJSDrawTest, DrawRectFilled) {
    JSValue r = eval("drawRectFilled(0, 0, 200, 100, 'green', 8.0)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 8.0f); // rounding
}

// drawCircle
TEST_F(QuickJSDrawTest, DrawCircle) {
    JSValue r = eval("drawCircle(50, 50, 25, '#fff', 2.0, 32)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawCircle");
    EXPECT_FLOAT_EQ(call.floatArgs[0], 50.0f);  // cx
    EXPECT_FLOAT_EQ(call.floatArgs[1], 50.0f);  // cy
    EXPECT_FLOAT_EQ(call.floatArgs[2], 25.0f);  // radius
    EXPECT_FLOAT_EQ(call.floatArgs[3], 2.0f);   // thickness
    EXPECT_FLOAT_EQ(call.floatArgs[4], 32.0f);  // segments
}

// drawTriangle
TEST_F(QuickJSDrawTest, DrawTriangle) {
    JSValue r = eval("drawTriangle(0, 0, 100, 0, 50, 86, '#ff0')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawTriangle");
    EXPECT_FLOAT_EQ(call.floatArgs[0], 0.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[1], 0.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[2], 100.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[3], 0.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[4], 50.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[5], 86.0f);
}

// drawTriangleFilled
TEST_F(QuickJSDrawTest, DrawTriangleFilled) {
    JSValue r = eval("drawTriangleFilled(10, 10, 20, 10, 15, 20, 'red')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawTriangleFilled");
}

// drawText records text content
TEST_F(QuickJSDrawTest, DrawText) {
    JSValue r = eval("drawText(10, 20, '#ffffff', 'Hello World')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawText");
    EXPECT_FLOAT_EQ(call.floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[1], 20.0f);
    EXPECT_EQ(call.textArg, "Hello World");
}

// drawPolyline with flat array
TEST_F(QuickJSDrawTest, DrawPolyline) {
    JSValue r = eval("drawPolyline([10, 20, 30, 40, 50, 60], '#0f0', true, 2.0)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawPolyline");
    // 3 points x 2 coords + closed flag + thickness = 8 floats
    ASSERT_EQ(call.floatArgs.size(), 8u);
    EXPECT_FLOAT_EQ(call.floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[1], 20.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[2], 30.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[3], 40.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[4], 50.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[5], 60.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[6], 1.0f);  // closed=true
    EXPECT_FLOAT_EQ(call.floatArgs[7], 2.0f);  // thickness
}

// drawBezierCubic
TEST_F(QuickJSDrawTest, DrawBezierCubic) {
    JSValue r = eval("drawBezierCubic(0,0, 50,100, 150,100, 200,0, '#f00', 2.0)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawBezierCubic");
    EXPECT_FLOAT_EQ(call.floatArgs[0], 0.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[1], 0.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[6], 200.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[7], 0.0f);
    EXPECT_FLOAT_EQ(call.floatArgs[8], 2.0f); // thickness
}

// drawNgon
TEST_F(QuickJSDrawTest, DrawNgon) {
    JSValue r = eval("drawNgon(100, 100, 50, '#fff', 6, 2.0)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawNgon");
    EXPECT_FLOAT_EQ(call.floatArgs[2], 50.0f);  // radius
    EXPECT_FLOAT_EQ(call.floatArgs[3], 6.0f);   // numSegments
    EXPECT_FLOAT_EQ(call.floatArgs[4], 2.0f);   // thickness
}

// drawNgonFilled
TEST_F(QuickJSDrawTest, DrawNgonFilled) {
    JSValue r = eval("drawNgonFilled(100, 100, 50, 'cyan', 8)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawNgonFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 8.0f); // numSegments
}

// drawEllipse
TEST_F(QuickJSDrawTest, DrawEllipse) {
    JSValue r = eval("drawEllipse(100, 100, 60, 30, '#f0f', 2.0, 0.5)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawEllipse");
    EXPECT_FLOAT_EQ(call.floatArgs[2], 60.0f);  // rx
    EXPECT_FLOAT_EQ(call.floatArgs[3], 30.0f);  // ry
    EXPECT_FLOAT_EQ(call.floatArgs[4], 2.0f);   // thickness
    EXPECT_FLOAT_EQ(call.floatArgs[5], 0.5f);   // rotation
}

// drawEllipseFilled
TEST_F(QuickJSDrawTest, DrawEllipseFilled) {
    JSValue r = eval("drawEllipseFilled(100, 100, 60, 30, '#0ff', 1.57)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawEllipseFilled");
    EXPECT_FLOAT_EQ(call.floatArgs[4], 1.57f); // rotation
}

// Null drawList doesn't crash - all functions should be safe
TEST_F(QuickJSDrawTest, NullDrawListNoCrash) {
    const char* calls[] = {
        "drawLine(0,0,1,1,'#fff')",
        "drawRect(0,0,1,1,'#fff')",
        "drawRectFilled(0,0,1,1,'#fff')",
        "drawCircle(0,0,1,'#fff')",
        "drawCircleFilled(0,0,1,'#fff')",
        "drawTriangle(0,0,1,0,0,1,'#fff')",
        "drawTriangleFilled(0,0,1,0,0,1,'#fff')",
        "drawText(0,0,'#fff','hi')",
        "drawPolyline([0,0,1,1],'#fff')",
        "drawBezierCubic(0,0,1,1,2,2,3,3,'#fff')",
        "drawNgon(0,0,1,'#fff',6)",
        "drawNgonFilled(0,0,1,'#fff',6)",
        "drawEllipse(0,0,1,1,'#fff')",
        "drawEllipseFilled(0,0,1,1,'#fff')",
        "drawImage('t',0,0,1,1)",
    };

    for (const char* code : calls) {
        JSValue r = eval(code);
        EXPECT_FALSE(JS_IsException(r)) << "Exception in: " << code;
        JS_FreeValue(ctx, r);
    }

    EXPECT_EQ(dc.recorded.size(), 15u);
}

// Insufficient args returns undefined without crashing
TEST_F(QuickJSDrawTest, InsufficientArgsNoCrash) {
    JSValue r = eval("drawLine(1, 2)"); // needs 5 args
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    EXPECT_EQ(dc.recorded.size(), 0u);
}

// Multiple draw calls accumulate
TEST_F(QuickJSDrawTest, MultipleCalls) {
    const char* code =
        "drawCircleFilled(10, 10, 5, 'red');"
        "drawLine(0, 0, 100, 100, 'blue', 2);"
        "drawText(50, 50, 'green', 'test');";
    JSValue r = eval(code);
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 3u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");
    EXPECT_EQ(dc.recorded[1].function, "drawLine");
    EXPECT_EQ(dc.recorded[2].function, "drawText");
}

// parseCSSColor standalone test
TEST_F(QuickJSDrawTest, ParseCSSColorValues) {
    using QuickJSDrawBindings::parseCSSColor;

    EXPECT_EQ(parseCSSColor("#ff0000"), IM_COL32(255, 0, 0, 255));
    EXPECT_EQ(parseCSSColor("#00ff00"), IM_COL32(0, 255, 0, 255));
    EXPECT_EQ(parseCSSColor("#0000ff"), IM_COL32(0, 0, 255, 255));
    EXPECT_EQ(parseCSSColor("#ffffff"), IM_COL32(255, 255, 255, 255));
    EXPECT_EQ(parseCSSColor("#000000"), IM_COL32(0, 0, 0, 255));
    EXPECT_EQ(parseCSSColor("red"),     IM_COL32(255, 0, 0, 255));
    EXPECT_EQ(parseCSSColor("white"),   IM_COL32(255, 255, 255, 255));
}

// Offset applied to rect coordinates
TEST_F(QuickJSDrawTest, OffsetAppliedToRect) {
    dc.offset = {50, 75};

    JSValue r = eval("drawRect(10, 20, 100, 50, '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 60.0f);  // 10 + 50
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 95.0f);  // 20 + 75
}

// Offset applied to circle center
TEST_F(QuickJSDrawTest, OffsetAppliedToCircle) {
    dc.offset = {10, 20};

    JSValue r = eval("drawCircle(50, 60, 25, '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 60.0f);  // 50 + 10
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 80.0f);  // 60 + 20
}

// Offset applied to polyline points
TEST_F(QuickJSDrawTest, OffsetAppliedToPolyline) {
    dc.offset = {5, 10};

    JSValue r = eval("drawPolyline([0, 0, 100, 200], '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 5.0f);    // 0 + 5
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 10.0f);   // 0 + 10
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 105.0f);  // 100 + 5
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 210.0f);  // 200 + 10
}

// --- Color parsing edge cases ---

// rgba() format parses RGB correctly; alpha is discarded by HEXAtoIV4 (hardcoded to 1.0)
TEST_F(QuickJSDrawTest, ColorParsingRGBA) {
    JSValue r = eval("drawCircleFilled(0, 0, 10, 'rgba(255,0,0,0.5)')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    ImU32 color = dc.recorded[0].color;
    // HEXAtoIV4(string) passes a=1.0f to RGBAtoIV4, discarding the rgba alpha
    ImU32 expected = IM_COL32(255, 0, 0, 255);
    EXPECT_EQ(color, expected);
}

// Invalid color string falls back to white
TEST_F(QuickJSDrawTest, ColorParsingInvalid) {
    using QuickJSDrawBindings::parseCSSColor;
    EXPECT_EQ(parseCSSColor("notacolor"), IM_COL32(255, 255, 255, 255));
}

// Empty string falls back to white
TEST_F(QuickJSDrawTest, ColorParsingEmptyString) {
    using QuickJSDrawBindings::parseCSSColor;
    EXPECT_EQ(parseCSSColor(""), IM_COL32(255, 255, 255, 255));
}

// Short hex #f00 parses as red
TEST_F(QuickJSDrawTest, ColorParsingShortHex) {
    using QuickJSDrawBindings::parseCSSColor;
    EXPECT_EQ(parseCSSColor("#f00"), IM_COL32(255, 0, 0, 255));
}

// --- Offset edge cases ---

// Offset applied to all 3 triangle vertices
TEST_F(QuickJSDrawTest, OffsetAppliedToTriangle) {
    dc.offset = {10, 20};

    JSValue r = eval("drawTriangle(0, 0, 100, 0, 50, 86, '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& args = dc.recorded[0].floatArgs;
    EXPECT_FLOAT_EQ(args[0], 10.0f);   // x1 + 10
    EXPECT_FLOAT_EQ(args[1], 20.0f);   // y1 + 20
    EXPECT_FLOAT_EQ(args[2], 110.0f);  // x2 + 10
    EXPECT_FLOAT_EQ(args[3], 20.0f);   // y2 + 20
    EXPECT_FLOAT_EQ(args[4], 60.0f);   // x3 + 10
    EXPECT_FLOAT_EQ(args[5], 106.0f);  // y3 + 20
}

// Offset applied to ellipse center, radii unchanged
TEST_F(QuickJSDrawTest, OffsetAppliedToEllipse) {
    dc.offset = {15, 25};

    JSValue r = eval("drawEllipse(100, 100, 60, 30, '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& args = dc.recorded[0].floatArgs;
    EXPECT_FLOAT_EQ(args[0], 115.0f);  // cx + 15
    EXPECT_FLOAT_EQ(args[1], 125.0f);  // cy + 25
    EXPECT_FLOAT_EQ(args[2], 60.0f);   // rx unchanged
    EXPECT_FLOAT_EQ(args[3], 30.0f);   // ry unchanged
}

// Offset applied to all 4 bezier control points
TEST_F(QuickJSDrawTest, OffsetAppliedToBezierCubic) {
    dc.offset = {5, 10};

    JSValue r = eval("drawBezierCubic(0,0, 50,100, 150,100, 200,0, '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& args = dc.recorded[0].floatArgs;
    EXPECT_FLOAT_EQ(args[0], 5.0f);    // p1.x + 5
    EXPECT_FLOAT_EQ(args[1], 10.0f);   // p1.y + 10
    EXPECT_FLOAT_EQ(args[2], 55.0f);   // p2.x + 5
    EXPECT_FLOAT_EQ(args[3], 110.0f);  // p2.y + 10
    EXPECT_FLOAT_EQ(args[4], 155.0f);  // p3.x + 5
    EXPECT_FLOAT_EQ(args[5], 110.0f);  // p3.y + 10
    EXPECT_FLOAT_EQ(args[6], 205.0f);  // p4.x + 5
    EXPECT_FLOAT_EQ(args[7], 10.0f);   // p4.y + 10
}

// Offset applied to ngon center, radius unchanged
TEST_F(QuickJSDrawTest, OffsetAppliedToNgon) {
    dc.offset = {20, 30};

    JSValue r = eval("drawNgon(100, 100, 50, '#fff', 6)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& args = dc.recorded[0].floatArgs;
    EXPECT_FLOAT_EQ(args[0], 120.0f);  // cx + 20
    EXPECT_FLOAT_EQ(args[1], 130.0f);  // cy + 30
    EXPECT_FLOAT_EQ(args[2], 50.0f);   // radius unchanged
}

// Offset applied to text position
TEST_F(QuickJSDrawTest, OffsetAppliedToText) {
    dc.offset = {7, 13};

    JSValue r = eval("drawText(10, 20, '#fff', 'hello')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 17.0f);  // 10 + 7
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 33.0f);  // 20 + 13
    EXPECT_EQ(dc.recorded[0].textArg, "hello");
}

// --- Default parameter tests ---

// drawRectFilled with rounding omitted defaults to 0
TEST_F(QuickJSDrawTest, DrawRectFilledDefaultRounding) {
    JSValue r = eval("drawRectFilled(0, 0, 100, 50, '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 0.0f); // rounding defaults to 0
}

// drawCircle with thickness and segments omitted
TEST_F(QuickJSDrawTest, DrawCircleDefaultParams) {
    JSValue r = eval("drawCircle(50, 50, 25, '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& args = dc.recorded[0].floatArgs;
    EXPECT_FLOAT_EQ(args[3], 1.0f);  // thickness defaults to 1.0
    EXPECT_FLOAT_EQ(args[4], 0.0f);  // segments defaults to 0
}

// drawEllipse with thickness and rotation omitted
TEST_F(QuickJSDrawTest, DrawEllipseDefaultParams) {
    JSValue r = eval("drawEllipse(50, 50, 30, 20, '#fff')");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& args = dc.recorded[0].floatArgs;
    EXPECT_FLOAT_EQ(args[4], 1.0f);  // thickness defaults to 1.0
    EXPECT_FLOAT_EQ(args[5], 0.0f);  // rotation defaults to 0.0
}

// --- drawImage tests ---

// drawImage is registered as a function
TEST_F(QuickJSDrawTest, DrawImageRegistered) {
    JSValue result = eval("typeof drawImage");
    ASSERT_FALSE(JS_IsException(result));
    const char* typeStr = JS_ToCString(ctx, result);
    EXPECT_STREQ(typeStr, "function");
    JS_FreeCString(ctx, typeStr);
    JS_FreeValue(ctx, result);
}

// drawImage with 5 args records textArg, position, size, and default UVs
TEST_F(QuickJSDrawTest, DrawImageBasic) {
    JSValue r = eval("drawImage('background', 10, 20, 200, 100)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawImage");
    EXPECT_EQ(call.textArg, "background");
    EXPECT_FLOAT_EQ(call.floatArgs[0], 10.0f);  // x
    EXPECT_FLOAT_EQ(call.floatArgs[1], 20.0f);  // y
    EXPECT_FLOAT_EQ(call.floatArgs[2], 200.0f); // w
    EXPECT_FLOAT_EQ(call.floatArgs[3], 100.0f); // h
    EXPECT_FLOAT_EQ(call.floatArgs[4], 0.0f);   // uvX0 default
    EXPECT_FLOAT_EQ(call.floatArgs[5], 0.0f);   // uvY0 default
    EXPECT_FLOAT_EQ(call.floatArgs[6], 1.0f);   // uvX1 default
    EXPECT_FLOAT_EQ(call.floatArgs[7], 1.0f);   // uvY1 default
}

// drawImage with 9 args records custom UV values
TEST_F(QuickJSDrawTest, DrawImageWithUVs) {
    JSValue r = eval("drawImage('sprite', 0, 0, 64, 64, 0.25, 0.25, 0.75, 0.75)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& call = dc.recorded[0];
    EXPECT_EQ(call.function, "drawImage");
    EXPECT_EQ(call.textArg, "sprite");
    EXPECT_FLOAT_EQ(call.floatArgs[4], 0.25f);  // uvX0
    EXPECT_FLOAT_EQ(call.floatArgs[5], 0.25f);  // uvY0
    EXPECT_FLOAT_EQ(call.floatArgs[6], 0.75f);  // uvX1
    EXPECT_FLOAT_EQ(call.floatArgs[7], 0.75f);  // uvY1
}

// Offset applied to x,y; w,h unchanged
TEST_F(QuickJSDrawTest, DrawImageOffsetApplied) {
    dc.offset = {50, 75};

    JSValue r = eval("drawImage('tex', 10, 20, 100, 50)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    auto& args = dc.recorded[0].floatArgs;
    EXPECT_FLOAT_EQ(args[0], 60.0f);   // 10 + 50
    EXPECT_FLOAT_EQ(args[1], 95.0f);   // 20 + 75
    EXPECT_FLOAT_EQ(args[2], 100.0f);  // w unchanged
    EXPECT_FLOAT_EQ(args[3], 50.0f);   // h unchanged
}

// Null drawList with recording works fine
TEST_F(QuickJSDrawTest, DrawImageNullDrawList) {
    JSValue r = eval("drawImage('tex', 0, 0, 1, 1)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawImage");
}

// Insufficient args (<5) produces no recording and no crash
TEST_F(QuickJSDrawTest, DrawImageInsufficientArgs) {
    JSValue r = eval("drawImage('tex', 0, 0)");
    ASSERT_FALSE(JS_IsException(r));
    JS_FreeValue(ctx, r);

    EXPECT_EQ(dc.recorded.size(), 0u);
}
