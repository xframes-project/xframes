#include <gtest/gtest.h>
#include <cmath>

#include <sol/sol.hpp>
#include "draw_context.h"
#include "sol2_draw_bindings.h"

// Tests exercise LuaCanvas's Sol2 bindings directly (no ImGui context needed).
// Uses DrawContext recording mode to verify draw calls.

class LuaCanvasTest : public ::testing::Test {
protected:
    sol::state lua;
    DrawContext dc;
    sol::protected_function renderFunc;
    bool hasRenderFunc = false;

    void SetUp() override {
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

        dc.drawList = nullptr;
        dc.offset = {0, 0};
        dc.recording = true;
        dc.recorded.clear();

        Sol2DrawBindings::registerDrawBindings(lua, dc);
    }

    // Mimics LuaCanvas::SetScriptFromString
    bool setScript(const char* script) {
        hasRenderFunc = false;
        std::string wrapped = std::string("return function() ") + script + " end";
        auto result = lua.safe_script(wrapped, sol::script_pass_on_error);
        if (!result.valid()) {
            return false;
        }
        renderFunc = result.get<sol::protected_function>();
        hasRenderFunc = true;
        return true;
    }

    // Mimics LuaCanvas::HandleInternalOp("setData") via JSON string
    void setData(const std::string& luaCode) {
        lua.safe_script("data = " + luaCode);
    }

    // Mimics LuaCanvas::HandleInternalOp("clear")
    void clear() {
        hasRenderFunc = false;
        lua["data"] = sol::nil;
    }

    // Mimics LuaCanvas::Render()
    void callRender() {
        dc.recorded.clear();
        if (hasRenderFunc) {
            auto result = renderFunc();
            if (!result.valid()) {
                sol::error err = result;
                // Error silently consumed in tests (matches Canvas behavior)
            }
        }
    }
};

// --- Basic lifecycle ---

TEST_F(LuaCanvasTest, SetScriptStoresFunction) {
    EXPECT_TRUE(setScript("drawCircleFilled(50, 50, 25, 'red')"));
    EXPECT_TRUE(hasRenderFunc);
}

TEST_F(LuaCanvasTest, SetScriptInvalidLua) {
    EXPECT_FALSE(setScript("function broken syntax !!!"));
    EXPECT_FALSE(hasRenderFunc);
}

TEST_F(LuaCanvasTest, RenderExecutesScript) {
    setScript("drawCircleFilled(50, 50, 25, 'red')");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 25.0f);
}

TEST_F(LuaCanvasTest, RenderWithNoScript) {
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

// --- Data handling ---

TEST_F(LuaCanvasTest, SetDataAccessibleInScript) {
    setData("{x = 100, y = 200, color = '#ff0000'}");
    setScript("drawCircleFilled(data.x, data.y, 10, data.color)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 100.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 200.0f);
}

TEST_F(LuaCanvasTest, SetDataArray) {
    setData("{{x=10, y=20}, {x=30, y=40}}");
    setScript(
        "for i = 1, #data do "
        "  drawCircleFilled(data[i].x, data[i].y, 5, 'blue') "
        "end"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 2u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[1].floatArgs[0], 30.0f);
}

TEST_F(LuaCanvasTest, DataPersistsBetweenFrames) {
    setData("{count = 3}");
    setScript(
        "for i = 0, data.count - 1 do "
        "  drawLine(i * 10, 0, i * 10, 100, '#fff') "
        "end"
    );

    callRender();
    ASSERT_EQ(dc.recorded.size(), 3u);

    callRender();
    ASSERT_EQ(dc.recorded.size(), 3u);
}

TEST_F(LuaCanvasTest, SetDataUpdates) {
    setData("{radius = 10}");
    setScript("drawCircleFilled(50, 50, data.radius, 'red')");

    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 10.0f);

    setData("{radius = 25}");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 25.0f);
}

TEST_F(LuaCanvasTest, ClearResetsState) {
    setData("{x = 42}");
    setScript("drawCircleFilled(data.x, 0, 5, 'red')");

    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);

    clear();

    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
    EXPECT_FALSE(hasRenderFunc);
}

// --- Draw function tests ---

TEST_F(LuaCanvasTest, DrawLine) {
    setScript("drawLine(10, 20, 30, 40, '#ffffff', 2)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawLine");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 20.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 30.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 40.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 2.0f);
}

TEST_F(LuaCanvasTest, DrawLineDefaultThickness) {
    setScript("drawLine(0, 0, 100, 100, 'red')");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 1.0f);
}

TEST_F(LuaCanvasTest, DrawRect) {
    setScript("drawRect(5, 10, 100, 50, 'blue', 3, 8)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRect");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 5.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 100.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 3.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[5], 8.0f);
}

TEST_F(LuaCanvasTest, DrawRectFilled) {
    setScript("drawRectFilled(0, 0, 200, 100, '#333333')");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 200.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 100.0f);
}

TEST_F(LuaCanvasTest, DrawCircle) {
    setScript("drawCircle(50, 50, 30, 'green', 2, 16)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircle");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 30.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 2.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 16.0f);
}

TEST_F(LuaCanvasTest, DrawTriangle) {
    setScript("drawTriangle(0, 0, 100, 0, 50, 86, 'yellow', 2)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawTriangle");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[5], 86.0f);
}

TEST_F(LuaCanvasTest, DrawTriangleFilled) {
    setScript("drawTriangleFilled(0, 0, 100, 0, 50, 86, 'orange')");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawTriangleFilled");
}

TEST_F(LuaCanvasTest, DrawText) {
    setScript("drawText(10, 20, '#00ff00', 'Hello Lua')");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawText");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 20.0f);
    EXPECT_EQ(dc.recorded[0].textArg, "Hello Lua");
}

TEST_F(LuaCanvasTest, DrawPolyline) {
    setScript("drawPolyline({10, 20, 30, 40, 50, 60}, 'red', true, 2)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawPolyline");
    // 3 points (6 coords) + closed flag + thickness = 8 floats
    ASSERT_EQ(dc.recorded[0].floatArgs.size(), 8u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 20.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[5], 60.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[6], 1.0f); // closed
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[7], 2.0f); // thickness
}

TEST_F(LuaCanvasTest, DrawBezierCubic) {
    setScript("drawBezierCubic(0, 0, 50, 0, 50, 100, 100, 100, 'purple', 3)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawBezierCubic");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[8], 3.0f); // thickness
}

TEST_F(LuaCanvasTest, DrawNgon) {
    setScript("drawNgon(50, 50, 30, 'cyan', 6, 2)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawNgon");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 6.0f); // segments
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 2.0f); // thickness
}

TEST_F(LuaCanvasTest, DrawNgonFilled) {
    setScript("drawNgonFilled(50, 50, 30, 'magenta', 8)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawNgonFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 8.0f);
}

TEST_F(LuaCanvasTest, DrawEllipse) {
    setScript("drawEllipse(100, 100, 40, 20, 'white', 2, 0.5)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawEllipse");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 40.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 20.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[5], 0.5f); // rotation
}

TEST_F(LuaCanvasTest, DrawEllipseFilled) {
    setScript("drawEllipseFilled(100, 100, 40, 20, 'pink')");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawEllipseFilled");
}

TEST_F(LuaCanvasTest, DrawConvexPolyFilled) {
    setScript("drawConvexPolyFilled({0, 0, 100, 0, 100, 100, 0, 100}, 'green')");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawConvexPolyFilled");
    ASSERT_EQ(dc.recorded[0].floatArgs.size(), 8u);
}

TEST_F(LuaCanvasTest, DrawImage) {
    setScript("drawImage('bg', 0, 0, 200, 100)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawImage");
    EXPECT_EQ(dc.recorded[0].textArg, "bg");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 200.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 100.0f);
}

TEST_F(LuaCanvasTest, MeasureText) {
    setScript(
        "local m = measureText('Hello')\n"
        "drawRectFilled(0, 0, m.width, m.height, 'red')"
    );
    callRender();

    // measureText + drawRectFilled
    ASSERT_GE(dc.recorded.size(), 2u);

    // Check measureText was recorded
    EXPECT_EQ(dc.recorded[0].function, "measureText");
    EXPECT_GT(dc.recorded[0].floatArgs[0], 0.0f); // width > 0
    EXPECT_GT(dc.recorded[0].floatArgs[1], 0.0f); // height > 0

    // drawRectFilled used the measured dimensions
    EXPECT_EQ(dc.recorded[1].function, "drawRectFilled");
    EXPECT_FLOAT_EQ(dc.recorded[1].floatArgs[2], dc.recorded[0].floatArgs[0]); // width matches
}

TEST_F(LuaCanvasTest, PushPopClipRect) {
    setScript(
        "pushClipRect(10, 10, 100, 100)\n"
        "drawRectFilled(0, 0, 200, 200, 'red')\n"
        "popClipRect()"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 3u);
    EXPECT_EQ(dc.recorded[0].function, "pushClipRect");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_EQ(dc.recorded[1].function, "drawRectFilled");
    EXPECT_EQ(dc.recorded[2].function, "popClipRect");
}

// --- Offset ---

TEST_F(LuaCanvasTest, OffsetAppliedDuringRender) {
    dc.offset = {100, 200};
    setScript("drawCircleFilled(10, 20, 5, 'red')");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 110.0f); // 10 + 100
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 220.0f); // 20 + 200
}

// --- Script edge cases ---

TEST_F(LuaCanvasTest, ScriptMultipleDrawCalls) {
    setScript(
        "drawRectFilled(0, 0, 200, 100, '#333')\n"
        "drawLine(0, 50, 200, 50, '#fff', 2)\n"
        "drawText(10, 10, '#0f0', 'LuaCanvas')"
    );
    callRender();

    ASSERT_EQ(dc.recorded.size(), 3u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
    EXPECT_EQ(dc.recorded[1].function, "drawLine");
    EXPECT_EQ(dc.recorded[2].function, "drawText");
    EXPECT_EQ(dc.recorded[2].textArg, "LuaCanvas");
}

TEST_F(LuaCanvasTest, ReplaceScript) {
    setScript("drawCircleFilled(0, 0, 5, 'red')");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");

    setScript("drawRect(0, 0, 100, 100, 'blue')");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRect");
}

TEST_F(LuaCanvasTest, ScriptRuntimeException) {
    EXPECT_TRUE(setScript("foo.bar()"));
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

TEST_F(LuaCanvasTest, EmptyScript) {
    EXPECT_TRUE(setScript(""));
    EXPECT_TRUE(hasRenderFunc);
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

TEST_F(LuaCanvasTest, LargeNumberOfDrawCalls) {
    setScript(
        "for i = 0, 999 do "
        "  drawLine(i, 0, i, 100, '#fff') "
        "end"
    );
    callRender();
    EXPECT_EQ(dc.recorded.size(), 1000u);
}

TEST_F(LuaCanvasTest, NestedData) {
    setData("{position = {x = 42, y = 84}, style = {color = '#ff0000'}}");
    setScript("drawCircleFilled(data.position.x, data.position.y, 5, data.style.color)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 42.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 84.0f);
}

TEST_F(LuaCanvasTest, ClearThenSetScript) {
    setScript("drawCircleFilled(0, 0, 5, 'red')");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);

    clear();
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);

    setScript("drawRect(0, 0, 100, 50, 'blue')");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRect");
}

// --- Canvas dimensions ---

TEST_F(LuaCanvasTest, CanvasDimensions) {
    lua["canvasWidth"] = 400.0f;
    lua["canvasHeight"] = 300.0f;

    setScript("drawRectFilled(0, 0, canvasWidth, canvasHeight, '#000')");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 400.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 300.0f);
}

// --- Lua math library ---

TEST_F(LuaCanvasTest, MathLibraryAvailable) {
    setScript(
        "local cx, cy = 100, 100\n"
        "local r = 50\n"
        "for i = 0, 11 do\n"
        "  local angle = (i * math.pi) / 6\n"
        "  local x = cx + r * math.cos(angle)\n"
        "  local y = cy + r * math.sin(angle)\n"
        "  drawCircleFilled(x, y, 3, 'white')\n"
        "end"
    );
    callRender();
    EXPECT_EQ(dc.recorded.size(), 12u);
}
