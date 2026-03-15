#include <gtest/gtest.h>

extern "C" {
#include <quickjs.h>
}

#include "quickjs_draw_bindings.h"

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
