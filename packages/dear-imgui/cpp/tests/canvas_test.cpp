#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include <quickjs.h>
}

#include "quickjs_draw_bindings.h"
#include "../app/include/canvas2d_shim.h"

// Tests exercise Canvas's QuickJS lifecycle directly (no ImGui context needed).
// Uses DrawContext recording mode to verify draw calls.

class CanvasTest : public ::testing::Test {
protected:
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;
    DrawContext dc;
    JSValue renderFunc = JS_UNDEFINED;

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
        if (ctx) {
            if (!JS_IsUndefined(renderFunc)) {
                JS_FreeValue(ctx, renderFunc);
            }
            JS_FreeContext(ctx);
        }
        if (rt) JS_FreeRuntime(rt);
    }

    // Mimics Canvas::HandleInternalOp("setScript")
    bool setScript(const char* script) {
        if (!JS_IsUndefined(renderFunc)) {
            JS_FreeValue(ctx, renderFunc);
            renderFunc = JS_UNDEFINED;
        }

        std::string wrapped = std::string("(function() { ") + script + " })";
        JSValue val = JS_Eval(ctx, wrapped.c_str(), wrapped.size(), "<canvas>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(val)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
            JS_FreeValue(ctx, val);
            return false;
        }
        renderFunc = val;
        return true;
    }

    // Mimics Canvas::HandleInternalOp("setData")
    void setData(const std::string& jsonStr) {
        std::string code = "globalThis.data = " + jsonStr + ";";
        JSValue result = JS_Eval(ctx, code.c_str(), code.size(), "<data>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, result);
    }

    // Mimics Canvas::HandleInternalOp("clear")
    void clear() {
        if (!JS_IsUndefined(renderFunc)) {
            JS_FreeValue(ctx, renderFunc);
            renderFunc = JS_UNDEFINED;
        }
        const char* code = "globalThis.data = undefined;";
        JSValue result = JS_Eval(ctx, code, strlen(code), "<clear>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(ctx, result);
    }

    // Mimics Canvas::Render() — calls the stored render function
    void callRender() {
        dc.recorded.clear();
        if (!JS_IsUndefined(renderFunc) && JS_IsFunction(ctx, renderFunc)) {
            JSValue global = JS_GetGlobalObject(ctx);
            JSValue result = JS_Call(ctx, renderFunc, global, 0, nullptr);
            if (JS_IsException(result)) {
                JSValue exc = JS_GetException(ctx);
                JS_FreeValue(ctx, exc);
            }
            JS_FreeValue(ctx, result);
            JS_FreeValue(ctx, global);
        }
    }
};

// Runtime and context are created successfully
TEST_F(CanvasTest, RuntimeContextCreation) {
    EXPECT_NE(rt, nullptr);
    EXPECT_NE(ctx, nullptr);
}

// setScript compiles and stores a render function
TEST_F(CanvasTest, SetScriptStoresFunction) {
    EXPECT_TRUE(setScript("drawCircleFilled(50, 50, 25, 'red');"));
    EXPECT_TRUE(JS_IsFunction(ctx, renderFunc));
}

// setScript with invalid JS returns false
TEST_F(CanvasTest, SetScriptInvalidJS) {
    EXPECT_FALSE(setScript("function { broken syntax !!!"));
    EXPECT_TRUE(JS_IsUndefined(renderFunc));
}

// Calling render executes the stored script
TEST_F(CanvasTest, RenderExecutesScript) {
    setScript("drawCircleFilled(50, 50, 25, 'red');");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 25.0f);
}

// Render with no script is a no-op
TEST_F(CanvasTest, RenderWithNoScript) {
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

// setData makes data accessible in JS
TEST_F(CanvasTest, SetDataAccessibleInScript) {
    setData(R"({"x": 100, "y": 200, "color": "#ff0000"})");
    setScript("drawCircleFilled(data.x, data.y, 10, data.color);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 100.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 200.0f);
}

// setData with array
TEST_F(CanvasTest, SetDataArray) {
    setData(R"([{"x":10,"y":20},{"x":30,"y":40}])");
    setScript(
        "for (var i = 0; i < data.length; i++) {"
        "  drawCircleFilled(data[i].x, data[i].y, 5, 'blue');"
        "}"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 2u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[1].floatArgs[0], 30.0f);
}

// Data persists between frames
TEST_F(CanvasTest, DataPersistsBetweenFrames) {
    setData(R"({"count": 3})");
    setScript(
        "for (var i = 0; i < data.count; i++) {"
        "  drawLine(i * 10, 0, i * 10, 100, '#fff');"
        "}"
    );

    callRender();
    ASSERT_EQ(dc.recorded.size(), 3u);

    // Second frame — same data, same result
    callRender();
    ASSERT_EQ(dc.recorded.size(), 3u);
}

// setData updates existing data
TEST_F(CanvasTest, SetDataUpdates) {
    setData(R"({"radius": 10})");
    setScript("drawCircleFilled(50, 50, data.radius, 'red');");

    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 10.0f);

    // Update data
    setData(R"({"radius": 25})");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 25.0f);
}

// clear resets render function and data
TEST_F(CanvasTest, ClearResetsState) {
    setData(R"({"x": 42})");
    setScript("drawCircleFilled(data.x, 0, 5, 'red');");

    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);

    clear();

    // Render should be a no-op now
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
    EXPECT_TRUE(JS_IsUndefined(renderFunc));
}

// Script can use multiple draw calls
TEST_F(CanvasTest, ScriptMultipleDrawCalls) {
    setScript(
        "drawRectFilled(0, 0, 200, 100, '#333');"
        "drawLine(0, 50, 200, 50, '#fff', 2);"
        "drawText(10, 10, '#0f0', 'Canvas');"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 3u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
    EXPECT_EQ(dc.recorded[1].function, "drawLine");
    EXPECT_EQ(dc.recorded[2].function, "drawText");
    EXPECT_EQ(dc.recorded[2].textArg, "Canvas");
}

// Offset is applied during render
TEST_F(CanvasTest, OffsetAppliedDuringRender) {
    dc.offset = {100, 200};
    setScript("drawCircleFilled(10, 20, 5, 'red');");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 110.0f); // 10 + 100
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 220.0f); // 20 + 200
}

// Replacing script with a new one works
TEST_F(CanvasTest, ReplaceScript) {
    setScript("drawCircleFilled(0, 0, 5, 'red');");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");

    setScript("drawRect(0, 0, 100, 100, 'blue');");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRect");
}

// --- Script execution edge cases ---

// Script compiles but throws at runtime; no crash, no draw calls
TEST_F(CanvasTest, ScriptRuntimeException) {
    EXPECT_TRUE(setScript("foo.bar();"));
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

// Script reads data.x before setData is called; no crash
TEST_F(CanvasTest, ScriptAccessesUndefinedData) {
    EXPECT_TRUE(setScript(
        "if (typeof data !== 'undefined' && data.x) {"
        "  drawCircleFilled(data.x, 0, 5, 'red');"
        "}"
    ));
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

// Closure variable persists state across callRender() calls
TEST_F(CanvasTest, ScriptWithClosureState) {
    // The IIFE wrapping means we need a global counter since each call re-enters the IIFE
    const char* code = "globalThis.counter = (globalThis.counter || 0) + 1;";
    JSValue result = JS_Eval(ctx, code, strlen(code), "<setup>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(ctx, result);

    setScript(
        "globalThis.counter = (globalThis.counter || 0) + 1;"
        "for (var i = 0; i < globalThis.counter; i++) {"
        "  drawCircleFilled(i * 10, 0, 5, 'red');"
        "}"
    );

    callRender(); // counter=2 (setup made it 1, first render increments to 2)
    EXPECT_EQ(dc.recorded.size(), 2u);

    callRender(); // counter=3
    EXPECT_EQ(dc.recorded.size(), 3u);

    callRender(); // counter=4
    EXPECT_EQ(dc.recorded.size(), 4u);
}

// Empty script compiles and renders with zero draw calls
TEST_F(CanvasTest, EmptyScript) {
    EXPECT_TRUE(setScript(""));
    EXPECT_TRUE(JS_IsFunction(ctx, renderFunc));
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

// Script conditionally draws based on data field
TEST_F(CanvasTest, ScriptWithConditionalDrawing) {
    setScript(
        "if (typeof data !== 'undefined' && data.visible) {"
        "  drawCircleFilled(50, 50, 10, 'green');"
        "}"
    );

    setData(R"({"visible": false})");
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);

    setData(R"({"visible": true})");
    callRender();
    EXPECT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");
}

// 1000 draw calls in a loop; all recorded
TEST_F(CanvasTest, LargeNumberOfDrawCalls) {
    setScript(
        "for (var i = 0; i < 1000; i++) {"
        "  drawLine(i, 0, i, 100, '#fff');"
        "}"
    );
    callRender();
    EXPECT_EQ(dc.recorded.size(), 1000u);
}

// --- Data edge cases ---

// Nested JSON object, script accesses deep property
TEST_F(CanvasTest, SetDataWithNestedObjects) {
    setData(R"({"position": {"x": 42, "y": 84}, "style": {"color": "#ff0000"}})");
    setScript("drawCircleFilled(data.position.x, data.position.y, 5, data.style.color);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 42.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 84.0f);
}

// JSON with null fields, script handles gracefully
TEST_F(CanvasTest, SetDataWithNullValues) {
    setData(R"({"x": 50, "label": null})");
    setScript(
        "drawCircleFilled(data.x, 0, 5, 'red');"
        "if (data.label !== null) {"
        "  drawText(0, 0, '#fff', data.label);"
        "}"
    );
    callRender();
    EXPECT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");
}

// setData without setScript; callRender is a no-op, no crash
TEST_F(CanvasTest, SetDataWithoutScript) {
    setData(R"({"x": 100})");
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

// setData with empty object; script checks for missing properties
TEST_F(CanvasTest, SetDataEmptyObject) {
    setData("{}");
    setScript(
        "var r = data.radius || 10;"
        "drawCircleFilled(0, 0, r, 'red');"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 10.0f); // fallback radius
}

// --- Lifecycle edge cases ---

// clear() twice is idempotent, no crash
TEST_F(CanvasTest, ClearWhenAlreadyClear) {
    setScript("drawCircleFilled(0, 0, 5, 'red');");
    clear();
    clear(); // second clear should be safe
    EXPECT_TRUE(JS_IsUndefined(renderFunc));
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

// Full cycle: set→render→clear→set new→render
TEST_F(CanvasTest, SetScriptClearSetScriptCycle) {
    setScript("drawCircleFilled(0, 0, 5, 'red');");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");

    clear();
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);

    setScript("drawRect(0, 0, 100, 50, 'blue');");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRect");
}

// Update data 100 times then render once; last data is used
TEST_F(CanvasTest, MultipleRapidDataUpdates) {
    setScript("drawCircleFilled(0, 0, data.value, 'red');");

    for (int i = 0; i < 100; i++) {
        setData("{\"value\": " + std::to_string(i) + "}");
    }

    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 99.0f); // last value
}

// --- drawImage in Canvas ---

// Script calls drawImage; recorded with correct function name and args
TEST_F(CanvasTest, DrawImageInScript) {
    setScript("drawImage('bg', 0, 0, 200, 100);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawImage");
    EXPECT_EQ(dc.recorded[0].textArg, "bg");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 0.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 0.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 200.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 100.0f);
}

// textureId comes from data object
TEST_F(CanvasTest, DrawImageDataDriven) {
    setData(R"({"texId": "player_sprite"})");
    setScript("drawImage(data.texId, 10, 20, 32, 32);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].textArg, "player_sprite");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 20.0f);
}

// ============================================================
// Canvas 2D API Shim Tests
// ============================================================

class Canvas2DTest : public ::testing::Test {
protected:
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;
    DrawContext dc;
    JSValue renderFunc = JS_UNDEFINED;

    void SetUp() override {
        rt = JS_NewRuntime();
        ASSERT_NE(rt, nullptr);
        ctx = JS_NewContext(rt);
        ASSERT_NE(ctx, nullptr);

        dc.drawList = nullptr;
        dc.offset = {0, 0};
        dc.recording = true;
        dc.recorded.clear();
        dc.currentFont = nullptr;

        JS_SetContextOpaque(ctx, &dc);
        QuickJSDrawBindings::registerDrawBindings(ctx);

        // Evaluate the Canvas 2D shim
        const auto& shim = getCanvas2DShim();
        JSValue shimResult = JS_Eval(ctx, shim.c_str(), shim.size(),
                                     "<canvas2d_shim>", JS_EVAL_TYPE_GLOBAL);
        ASSERT_FALSE(JS_IsException(shimResult)) << "Canvas 2D shim failed to evaluate";
        JS_FreeValue(ctx, shimResult);

        // Set canvas dimensions
        JSValue global = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global, "__canvasWidth", JS_NewFloat64(ctx, 400));
        JS_SetPropertyStr(ctx, global, "__canvasHeight", JS_NewFloat64(ctx, 300));
        JS_FreeValue(ctx, global);
    }

    void TearDown() override {
        if (ctx) {
            if (!JS_IsUndefined(renderFunc)) {
                JS_FreeValue(ctx, renderFunc);
            }
            JS_FreeContext(ctx);
        }
        if (rt) JS_FreeRuntime(rt);
    }

    bool setScript(const char* script) {
        if (!JS_IsUndefined(renderFunc)) {
            JS_FreeValue(ctx, renderFunc);
            renderFunc = JS_UNDEFINED;
        }
        std::string wrapped = std::string("(function() { ") + script + " })";
        JSValue val = JS_Eval(ctx, wrapped.c_str(), wrapped.size(), "<canvas>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(val)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
            JS_FreeValue(ctx, val);
            return false;
        }
        renderFunc = val;
        return true;
    }

    void callRender() {
        dc.recorded.clear();
        if (!JS_IsUndefined(renderFunc) && JS_IsFunction(ctx, renderFunc)) {
            JSValue global = JS_GetGlobalObject(ctx);
            JSValue result = JS_Call(ctx, renderFunc, global, 0, nullptr);
            if (JS_IsException(result)) {
                JSValue exc = JS_GetException(ctx);
                JS_FreeValue(ctx, exc);
            }
            JS_FreeValue(ctx, result);
            JS_FreeValue(ctx, global);
        }
    }
};

// --- Stage 1: Core State Machine + Basic Drawing ---

TEST_F(Canvas2DTest, FillRectBasic) {
    setScript("ctx.fillStyle = 'red'; ctx.fillRect(10, 20, 100, 50);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 20.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 100.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 50.0f);
}

TEST_F(Canvas2DTest, StrokeRectBasic) {
    setScript("ctx.strokeStyle = 'blue'; ctx.lineWidth = 3; ctx.strokeRect(5, 5, 80, 40);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRect");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 5.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 5.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 80.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 40.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 3.0f); // thickness
}

TEST_F(Canvas2DTest, FillTextBasic) {
    setScript("ctx.fillStyle = '#00ff00'; ctx.fillText('Hello', 10, 50);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawText");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 50.0f);
    EXPECT_EQ(dc.recorded[0].textArg, "Hello");
}

TEST_F(Canvas2DTest, SaveRestoreReverts) {
    setScript(
        "ctx.fillStyle = 'red';"
        "ctx.save();"
        "ctx.fillStyle = 'blue';"
        "ctx.fillRect(0, 0, 10, 10);"  // blue
        "ctx.restore();"
        "ctx.fillRect(0, 0, 10, 10);"  // should be red again
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 2u);
    // First rect should use blue color
    auto blueColor = dc.recorded[0].color;
    // Second rect should use red color
    auto redColor = dc.recorded[1].color;
    EXPECT_NE(blueColor, redColor);
}

TEST_F(Canvas2DTest, CanvasDimensions) {
    setScript(
        "ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 400.0f); // width
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 300.0f); // height
}

TEST_F(Canvas2DTest, ClearRectFillsBlack) {
    setScript("ctx.clearRect(10, 10, 50, 50);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
}

TEST_F(Canvas2DTest, SaveRestoreLineWidth) {
    setScript(
        "ctx.lineWidth = 5;"
        "ctx.save();"
        "ctx.lineWidth = 10;"
        "ctx.strokeRect(0, 0, 50, 50);"
        "ctx.restore();"
        "ctx.strokeRect(0, 0, 50, 50);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 2u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[1].floatArgs[4], 5.0f);
}

TEST_F(Canvas2DTest, DefaultFillStyleIsBlack) {
    setScript("ctx.fillRect(0, 0, 10, 10);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    // Black: #000000 = IM_COL32(0,0,0,255) = 0xFF000000
    EXPECT_EQ(dc.recorded[0].color, IM_COL32(0, 0, 0, 255));
}

// --- Stage 2: Path API ---

TEST_F(Canvas2DTest, ArcStroke) {
    setScript(
        "ctx.strokeStyle = 'red';"
        "ctx.beginPath();"
        "ctx.arc(50, 50, 20, 0, Math.PI * 2);"
        "ctx.stroke();"
    );
    callRender();

    ASSERT_GE(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawPolyline");
    // Should have many points from tessellation
    // Points are stored as flat pairs + closed flag + thickness at end
    EXPECT_GT(dc.recorded[0].floatArgs.size(), 10u);
}

TEST_F(Canvas2DTest, TriangleFill) {
    setScript(
        "ctx.fillStyle = 'green';"
        "ctx.beginPath();"
        "ctx.moveTo(0, 0);"
        "ctx.lineTo(100, 0);"
        "ctx.lineTo(100, 100);"
        "ctx.closePath();"
        "ctx.fill();"
    );
    callRender();

    ASSERT_GE(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawConvexPolyFilled");
    // Should have 3 points (6 floats)
    EXPECT_EQ(dc.recorded[0].floatArgs.size(), 6u);
}

TEST_F(Canvas2DTest, BeginPathClears) {
    setScript(
        "ctx.beginPath();"
        "ctx.moveTo(0, 0);"
        "ctx.lineTo(100, 100);"
        "ctx.beginPath();"  // clears the path
        "ctx.stroke();"     // nothing to stroke
    );
    callRender();

    EXPECT_EQ(dc.recorded.size(), 0u);
}

TEST_F(Canvas2DTest, BezierCurveTessellation) {
    setScript(
        "ctx.beginPath();"
        "ctx.moveTo(0, 0);"
        "ctx.bezierCurveTo(50, 0, 50, 100, 100, 100);"
        "ctx.stroke();"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawPolyline");
    // 1 initial point + 20 tessellated = 21 points = 42 floats + closed + thickness
    EXPECT_GT(dc.recorded[0].floatArgs.size(), 20u);
}

TEST_F(Canvas2DTest, QuadraticCurveTessellation) {
    setScript(
        "ctx.beginPath();"
        "ctx.moveTo(0, 0);"
        "ctx.quadraticCurveTo(50, 100, 100, 0);"
        "ctx.stroke();"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawPolyline");
    EXPECT_GT(dc.recorded[0].floatArgs.size(), 10u);
}

TEST_F(Canvas2DTest, MultipleSubPaths) {
    setScript(
        "ctx.beginPath();"
        "ctx.moveTo(0, 0);"
        "ctx.lineTo(50, 50);"
        "ctx.moveTo(100, 0);"  // new sub-path
        "ctx.lineTo(150, 50);"
        "ctx.stroke();"
    );
    callRender();

    // Two sub-paths → two drawPolyline calls
    ASSERT_EQ(dc.recorded.size(), 2u);
    EXPECT_EQ(dc.recorded[0].function, "drawPolyline");
    EXPECT_EQ(dc.recorded[1].function, "drawPolyline");
}

// --- Stage 3: Transform Stack ---

TEST_F(Canvas2DTest, TranslateAffectsDrawing) {
    setScript(
        "ctx.translate(100, 0);"
        "ctx.fillRect(0, 0, 50, 50);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 100.0f); // x translated by 100
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 0.0f);
}

TEST_F(Canvas2DTest, RotateProducesConvexPoly) {
    setScript(
        "ctx.rotate(Math.PI / 4);"
        "ctx.fillRect(0, 0, 50, 50);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    // Rotation means we can't use drawRectFilled, must use drawConvexPolyFilled
    EXPECT_EQ(dc.recorded[0].function, "drawConvexPolyFilled");
    // 4 corners = 8 floats
    EXPECT_EQ(dc.recorded[0].floatArgs.size(), 8u);
}

TEST_F(Canvas2DTest, ScaleAffectsDrawing) {
    setScript(
        "ctx.scale(2, 3);"
        "ctx.fillRect(10, 10, 50, 50);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    // Scale is non-identity (scale != 1,1 on diagonal means not just translation)
    EXPECT_EQ(dc.recorded[0].function, "drawConvexPolyFilled");
    // First point should be (2*10, 3*10) = (20, 30)
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 20.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 30.0f);
}

TEST_F(Canvas2DTest, SaveRestoreTransform) {
    setScript(
        "ctx.save();"
        "ctx.translate(10, 20);"
        "ctx.restore();"
        "ctx.fillRect(0, 0, 10, 10);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 0.0f); // x should NOT be 10
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 0.0f); // y should NOT be 20
}

TEST_F(Canvas2DTest, ResetTransform) {
    setScript(
        "ctx.translate(100, 200);"
        "ctx.rotate(1);"
        "ctx.resetTransform();"
        "ctx.fillRect(5, 5, 10, 10);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 5.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 5.0f);
}

TEST_F(Canvas2DTest, SetTransform) {
    setScript(
        "ctx.setTransform(1, 0, 0, 1, 50, 50);"
        "ctx.fillRect(0, 0, 10, 10);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 50.0f);
}

TEST_F(Canvas2DTest, TranslateAffectsPath) {
    setScript(
        "ctx.translate(100, 200);"
        "ctx.beginPath();"
        "ctx.moveTo(0, 0);"
        "ctx.lineTo(50, 0);"
        "ctx.lineTo(50, 50);"
        "ctx.closePath();"
        "ctx.fill();"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawConvexPolyFilled");
    // First point at (100, 200)
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 100.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 200.0f);
}

TEST_F(Canvas2DTest, GetTransform) {
    setScript(
        "ctx.translate(10, 20);"
        "var t = ctx.getTransform();"
        "ctx.fillRect(t.e, t.f, 5, 5);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    // fillRect(10, 20, 5, 5) after transform(10,20) => x = 10+10 = 20, y = 20+20 = 40
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 20.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 40.0f);
}

// --- Stage 4: Text Measurement + Alignment ---

TEST_F(Canvas2DTest, MeasureTextReturnsPositiveWidth) {
    setScript(
        "var m = ctx.measureText('Hello World');"
        "ctx.fillRect(0, 0, m.width, 10);"
    );
    callRender();

    // __measureText is also recorded, so find the drawRectFilled entry
    bool found = false;
    for (auto& call : dc.recorded) {
        if (call.function == "drawRectFilled") {
            found = true;
            float width = call.floatArgs[2];
            EXPECT_GT(width, 0.0f);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(Canvas2DTest, TextAlignCenter) {
    setScript(
        "ctx.textAlign = 'center';"
        "ctx.fillText('Hi', 100, 50);"
    );
    callRender();

    // Should find __measureText + drawText in recorded
    // Look for the drawText call
    bool foundText = false;
    for (auto& call : dc.recorded) {
        if (call.function == "drawText") {
            foundText = true;
            // x should be less than 100 (shifted left by half width)
            EXPECT_LT(call.floatArgs[0], 100.0f);
            EXPECT_EQ(call.textArg, "Hi");
        }
    }
    EXPECT_TRUE(foundText);
}

TEST_F(Canvas2DTest, TextAlignRight) {
    setScript(
        "ctx.textAlign = 'right';"
        "ctx.fillText('Test', 200, 50);"
    );
    callRender();

    bool foundText = false;
    for (auto& call : dc.recorded) {
        if (call.function == "drawText") {
            foundText = true;
            // x should be less than 200 (shifted left by full width)
            EXPECT_LT(call.floatArgs[0], 200.0f);
        }
    }
    EXPECT_TRUE(foundText);
}

TEST_F(Canvas2DTest, TextBaselineMiddle) {
    setScript(
        "ctx.textBaseline = 'middle';"
        "ctx.fillText('Abc', 10, 100);"
    );
    callRender();

    bool foundText = false;
    for (auto& call : dc.recorded) {
        if (call.function == "drawText") {
            foundText = true;
            // y should be less than 100 (shifted up by half height)
            EXPECT_LT(call.floatArgs[1], 100.0f);
        }
    }
    EXPECT_TRUE(foundText);
}

TEST_F(Canvas2DTest, TextAlignLeftIsDefault) {
    setScript("ctx.fillText('X', 50, 50);");
    callRender();

    // Default textAlign is 'start' which equals 'left' for LTR — no offset
    bool foundText = false;
    for (auto& call : dc.recorded) {
        if (call.function == "drawText") {
            foundText = true;
            EXPECT_FLOAT_EQ(call.floatArgs[0], 50.0f);
        }
    }
    EXPECT_TRUE(foundText);
}

// --- Stage 5: Dashed Lines, rect(), Clip ---

TEST_F(Canvas2DTest, SetLineDashProducesMultiplePolylines) {
    setScript(
        "ctx.setLineDash([10, 5]);"
        "ctx.beginPath();"
        "ctx.moveTo(0, 0);"
        "ctx.lineTo(100, 0);"
        "ctx.stroke();"
    );
    callRender();

    // 100px line with 10-on 5-off = ~7 segments
    int polylineCount = 0;
    for (auto& call : dc.recorded) {
        if (call.function == "drawPolyline") polylineCount++;
    }
    EXPECT_GT(polylineCount, 1);
}

TEST_F(Canvas2DTest, GetLineDashReturnsSet) {
    setScript(
        "ctx.setLineDash([10, 5, 3]);"
        "var d = ctx.getLineDash();"
        "ctx.fillRect(0, 0, d.length, 1);"  // d.length = 3
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 3.0f); // width = dash array length
}

TEST_F(Canvas2DTest, EmptyLineDashMeansNoDash) {
    setScript(
        "ctx.setLineDash([10, 5]);"
        "ctx.setLineDash([]);"  // clear dashes
        "ctx.beginPath();"
        "ctx.moveTo(0, 0);"
        "ctx.lineTo(100, 0);"
        "ctx.stroke();"
    );
    callRender();

    int polylineCount = 0;
    for (auto& call : dc.recorded) {
        if (call.function == "drawPolyline") polylineCount++;
    }
    EXPECT_EQ(polylineCount, 1); // single continuous line
}

TEST_F(Canvas2DTest, RectPathMethod) {
    setScript(
        "ctx.beginPath();"
        "ctx.rect(10, 20, 80, 60);"
        "ctx.fill();"
    );
    callRender();

    ASSERT_GE(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawConvexPolyFilled");
    // rect() produces 4 points via moveTo+lineTo+closePath
}

TEST_F(Canvas2DTest, ClipPushesAndPopsClipRect) {
    setScript(
        "ctx.save();"
        "ctx.beginPath();"
        "ctx.rect(10, 10, 100, 100);"
        "ctx.clip();"
        "ctx.fillRect(0, 0, 200, 200);"
        "ctx.restore();"
    );
    callRender();

    // Should have: __pushClipRect, drawRectFilled, __popClipRect
    bool foundPush = false, foundPop = false, foundFill = false;
    for (auto& call : dc.recorded) {
        if (call.function == "__pushClipRect") {
            foundPush = true;
            EXPECT_FLOAT_EQ(call.floatArgs[0], 10.0f);
            EXPECT_FLOAT_EQ(call.floatArgs[1], 10.0f);
            EXPECT_FLOAT_EQ(call.floatArgs[2], 110.0f);
            EXPECT_FLOAT_EQ(call.floatArgs[3], 110.0f);
        }
        if (call.function == "__popClipRect") foundPop = true;
        if (call.function == "drawRectFilled") foundFill = true;
    }
    EXPECT_TRUE(foundPush);
    EXPECT_TRUE(foundPop);
    EXPECT_TRUE(foundFill);
}

TEST_F(Canvas2DTest, ClipOnlyPopsOnRestore) {
    setScript(
        "ctx.save();"
        "ctx.beginPath();"
        "ctx.rect(0, 0, 50, 50);"
        "ctx.clip();"
        "ctx.save();"
        "ctx.beginPath();"
        "ctx.rect(10, 10, 30, 30);"
        "ctx.clip();"
        "ctx.restore();"  // pops inner clip
        "ctx.restore();"  // pops outer clip
    );
    callRender();

    int pushCount = 0, popCount = 0;
    for (auto& call : dc.recorded) {
        if (call.function == "__pushClipRect") pushCount++;
        if (call.function == "__popClipRect") popCount++;
    }
    EXPECT_EQ(pushCount, 2);
    EXPECT_EQ(popCount, 2);
}

// --- Combined tests ---

TEST_F(Canvas2DTest, OffsetAppliedToCtxDrawing) {
    dc.offset = {50, 75};
    setScript("ctx.fillRect(10, 20, 30, 40);");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 60.0f);  // 10 + 50
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 95.0f);   // 20 + 75
}

TEST_F(Canvas2DTest, TranslateAndStrokeRectNoDash) {
    setScript(
        "ctx.translate(50, 50);"
        "ctx.strokeRect(0, 0, 100, 100);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    // Translation only → still uses drawRect
    EXPECT_EQ(dc.recorded[0].function, "drawRect");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 50.0f);
}

TEST_F(Canvas2DTest, CtxCoexistsWithRawDrawFunctions) {
    setScript(
        "drawCircleFilled(10, 10, 5, 'red');"
        "ctx.fillStyle = 'blue';"
        "ctx.fillRect(20, 20, 30, 30);"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 2u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");
    EXPECT_EQ(dc.recorded[1].function, "drawRectFilled");
}

TEST_F(Canvas2DTest, FontParsesPxSize) {
    setScript(
        "ctx.font = '24px Arial';"
        "ctx.fillText('Big', 0, 0);"
    );
    callRender();

    // Font parsing doesn't change ImGui font, but the font string is stored
    bool foundText = false;
    for (auto& call : dc.recorded) {
        if (call.function == "drawText") {
            foundText = true;
            EXPECT_EQ(call.textArg, "Big");
        }
    }
    EXPECT_TRUE(foundText);
}

TEST_F(Canvas2DTest, LineDashSavedAndRestored) {
    setScript(
        "ctx.setLineDash([10, 5]);"
        "ctx.save();"
        "ctx.setLineDash([1, 1]);"
        "ctx.restore();"
        // After restore, dash should be [10, 5] again
        "var d = ctx.getLineDash();"
        "ctx.fillRect(0, 0, d[0], d[1]);"  // w=10, h=5
    );
    callRender();

    ASSERT_GE(dc.recorded.size(), 1u);
    // Find the fillRect
    for (auto& call : dc.recorded) {
        if (call.function == "drawRectFilled") {
            EXPECT_FLOAT_EQ(call.floatArgs[2], 10.0f);
            EXPECT_FLOAT_EQ(call.floatArgs[3], 5.0f);
        }
    }
}

TEST_F(Canvas2DTest, ArcPartialSweep) {
    setScript(
        "ctx.beginPath();"
        "ctx.arc(50, 50, 20, 0, Math.PI);"  // half circle
        "ctx.stroke();"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawPolyline");
    // Should have tessellated points for a half circle
    EXPECT_GT(dc.recorded[0].floatArgs.size(), 6u);
}

TEST_F(Canvas2DTest, StrokeWithoutBeginPath) {
    // stroke() with no path does nothing
    setScript("ctx.stroke();");
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

TEST_F(Canvas2DTest, FillWithoutBeginPath) {
    // fill() with no path does nothing
    setScript("ctx.fill();");
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}
