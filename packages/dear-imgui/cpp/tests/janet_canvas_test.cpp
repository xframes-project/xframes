#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include <janet.h>
}

#include "draw_context.h"
#include "janet_draw_bindings.h"

// Tests exercise JanetCanvas's draw bindings directly (no ImGui context needed).
// Uses DrawContext recording mode to verify draw calls.

class JanetCanvasTest : public ::testing::Test {
protected:
    JanetTable* env = nullptr;
    DrawContext dc;
    Janet renderFuncValue = janet_wrap_nil();
    JanetFunction* renderFunc = nullptr;
    bool hasRenderFunc = false;

    // Update an existing janet_var binding in-place, or create it if it doesn't exist.
    void janetSetVar(const char* name, Janet value) {
        Janet sym = janet_csymbolv(name);
        Janet slot = janet_table_get(env, sym);
        if (janet_checktype(slot, JANET_TABLE)) {
            JanetTable* slotTable = janet_unwrap_table(slot);
            Janet ref = janet_table_get(slotTable, janet_ckeywordv("ref"));
            if (janet_checktype(ref, JANET_ARRAY)) {
                JanetArray* arr = janet_unwrap_array(ref);
                if (arr->count > 0) {
                    arr->data[0] = value;
                    return;
                }
            }
        }
        janet_var(env, name, value, NULL);
    }

    void SetUp() override {
        janet_init();
        env = janet_core_env(NULL);

        dc.drawList = nullptr;
        dc.offset = {0, 0};
        dc.recording = true;
        dc.recorded.clear();

        JanetDrawBindings::registerDrawBindings(env, dc);

        // Create mutable var bindings so compiled functions see updates
        janet_var(env, "data", janet_wrap_nil(), NULL);
        janet_var(env, "canvas-width", janet_wrap_number(0), NULL);
        janet_var(env, "canvas-height", janet_wrap_number(0), NULL);
    }

    void TearDown() override {
        if (hasRenderFunc) {
            janet_gcunroot(renderFuncValue);
        }
        janet_deinit();
    }

    // Mimics JanetCanvas::SetScriptFromString
    bool setScript(const char* script) {
        if (hasRenderFunc) {
            janet_gcunroot(renderFuncValue);
            hasRenderFunc = false;
            renderFunc = nullptr;
        }

        std::string wrapped = std::string("(fn [] ") + script + ")";
        Janet out;
        int status = janet_dostring(env, wrapped.c_str(), "test", &out);
        if (status != 0) {
            return false;
        }

        if (!janet_checktype(out, JANET_FUNCTION)) {
            return false;
        }

        renderFuncValue = out;
        renderFunc = janet_unwrap_function(out);
        janet_gcroot(renderFuncValue);
        hasRenderFunc = true;
        return true;
    }

    // Mimics JanetCanvas::HandleInternalOp("setData") — evaluates Janet expression and updates var binding
    void setData(const char* janetExpr) {
        Janet out;
        janet_dostring(env, janetExpr, "data", &out);
        janetSetVar("data", out);
    }

    // Mimics JanetCanvas::HandleInternalOp("clear")
    void clear() {
        if (hasRenderFunc) {
            janet_gcunroot(renderFuncValue);
        }
        hasRenderFunc = false;
        renderFunc = nullptr;
        renderFuncValue = janet_wrap_nil();
        janetSetVar("data", janet_wrap_nil());
    }

    // Mimics JanetCanvas::Render()
    void callRender() {
        dc.recorded.clear();
        if (hasRenderFunc && renderFunc) {
            Janet out;
            JanetFiber* fiber = nullptr;
            JanetSignal status = janet_pcall(renderFunc, 0, NULL, &out, &fiber);
            if (status != JANET_SIGNAL_OK) {
                // Error silently consumed in tests (matches Canvas behavior)
            }
        }
    }
};

// --- Basic lifecycle ---

TEST_F(JanetCanvasTest, SetScriptStoresFunction) {
    EXPECT_TRUE(setScript("(draw-circle-filled 50 50 25 \"red\")"));
    EXPECT_TRUE(hasRenderFunc);
}

TEST_F(JanetCanvasTest, SetScriptInvalidJanet) {
    EXPECT_FALSE(setScript("(defn broken syntax !!!"));
    EXPECT_FALSE(hasRenderFunc);
}

TEST_F(JanetCanvasTest, RenderExecutesScript) {
    setScript("(draw-circle-filled 50 50 25 \"red\")");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 25.0f);
}

TEST_F(JanetCanvasTest, RenderWithNoScript) {
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

// --- Data handling ---

TEST_F(JanetCanvasTest, SetDataAccessibleInScript) {
    setData("{:x 100 :y 200 :color \"#ff0000\"}");
    setScript("(draw-circle-filled (data :x) (data :y) 10 (data :color))");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 100.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 200.0f);
}

TEST_F(JanetCanvasTest, SetDataArray) {
    setData("@[{:x 10 :y 20} {:x 30 :y 40}]");
    setScript(R"(
        (each item data
            (draw-circle-filled (item :x) (item :y) 5 "blue"))
    )");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 2u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[1].floatArgs[0], 30.0f);
}

TEST_F(JanetCanvasTest, DataPersistsBetweenFrames) {
    setData("{:count 3}");
    setScript(R"(
        (for i 0 (data :count)
            (draw-line (* i 10) 0 (* i 10) 100 "#fff"))
    )");

    callRender();
    ASSERT_EQ(dc.recorded.size(), 3u);

    callRender();
    ASSERT_EQ(dc.recorded.size(), 3u);
}

TEST_F(JanetCanvasTest, SetDataUpdates) {
    setData("{:radius 10}");
    setScript("(draw-circle-filled 50 50 (data :radius) \"red\")");

    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 10.0f);

    setData("{:radius 25}");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 25.0f);
}

TEST_F(JanetCanvasTest, ClearResetsState) {
    setData("{:x 42}");
    setScript("(draw-circle-filled (data :x) 0 5 \"red\")");

    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);

    clear();

    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
    EXPECT_FALSE(hasRenderFunc);
}

// --- Draw function tests ---

TEST_F(JanetCanvasTest, DrawLine) {
    setScript("(draw-line 10 20 30 40 \"#ffffff\" 2)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawLine");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 20.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 30.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 40.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 2.0f);
}

TEST_F(JanetCanvasTest, DrawLineDefaultThickness) {
    setScript("(draw-line 0 0 100 100 \"red\")");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 1.0f);
}

TEST_F(JanetCanvasTest, DrawRect) {
    setScript("(draw-rect 5 10 100 50 \"blue\" 3 8)");
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

TEST_F(JanetCanvasTest, DrawRectFilled) {
    setScript("(draw-rect-filled 0 0 200 100 \"#333333\")");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 200.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 100.0f);
}

TEST_F(JanetCanvasTest, DrawCircle) {
    setScript("(draw-circle 50 50 30 \"green\" 2 16)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircle");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 30.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 2.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 16.0f);
}

TEST_F(JanetCanvasTest, DrawTriangle) {
    setScript("(draw-triangle 0 0 100 0 50 86 \"yellow\" 2)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawTriangle");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 50.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[5], 86.0f);
}

TEST_F(JanetCanvasTest, DrawTriangleFilled) {
    setScript("(draw-triangle-filled 0 0 100 0 50 86 \"orange\")");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawTriangleFilled");
}

TEST_F(JanetCanvasTest, DrawText) {
    setScript("(draw-text 10 20 \"#00ff00\" \"Hello Janet\")");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawText");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 20.0f);
    EXPECT_EQ(dc.recorded[0].textArg, "Hello Janet");
}

TEST_F(JanetCanvasTest, DrawPolyline) {
    setScript("(draw-polyline @[10 20 30 40 50 60] \"red\" true 2)");
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

TEST_F(JanetCanvasTest, DrawBezierCubic) {
    setScript("(draw-bezier-cubic 0 0 50 0 50 100 100 100 \"purple\" 3)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawBezierCubic");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[8], 3.0f); // thickness
}

TEST_F(JanetCanvasTest, DrawNgon) {
    setScript("(draw-ngon 50 50 30 \"cyan\" 6 2)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawNgon");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 6.0f); // segments
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[4], 2.0f); // thickness
}

TEST_F(JanetCanvasTest, DrawNgonFilled) {
    setScript("(draw-ngon-filled 50 50 30 \"magenta\" 8)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawNgonFilled");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 8.0f);
}

TEST_F(JanetCanvasTest, DrawEllipse) {
    setScript("(draw-ellipse 100 100 40 20 \"white\" 2 0.5)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawEllipse");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 40.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 20.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[5], 0.5f); // rotation
}

TEST_F(JanetCanvasTest, DrawEllipseFilled) {
    setScript("(draw-ellipse-filled 100 100 40 20 \"pink\")");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawEllipseFilled");
}

TEST_F(JanetCanvasTest, DrawConvexPolyFilled) {
    setScript("(draw-convex-poly-filled @[0 0 100 0 100 100 0 100] \"green\")");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawConvexPolyFilled");
    ASSERT_EQ(dc.recorded[0].floatArgs.size(), 8u);
}

TEST_F(JanetCanvasTest, DrawImage) {
    setScript("(draw-image \"bg\" 0 0 200 100)");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawImage");
    EXPECT_EQ(dc.recorded[0].textArg, "bg");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 200.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 100.0f);
}

TEST_F(JanetCanvasTest, MeasureText) {
    setScript(R"(
        (let [m (measure-text "Hello")]
            (draw-rect-filled 0 0 (m :width) (m :height) "red"))
    )");
    callRender();

    // measureText + drawRectFilled
    ASSERT_GE(dc.recorded.size(), 2u);

    EXPECT_EQ(dc.recorded[0].function, "measureText");
    EXPECT_GT(dc.recorded[0].floatArgs[0], 0.0f); // width > 0
    EXPECT_GT(dc.recorded[0].floatArgs[1], 0.0f); // height > 0

    EXPECT_EQ(dc.recorded[1].function, "drawRectFilled");
    EXPECT_FLOAT_EQ(dc.recorded[1].floatArgs[2], dc.recorded[0].floatArgs[0]); // width matches
}

TEST_F(JanetCanvasTest, PushPopClipRect) {
    setScript(R"(
        (push-clip-rect 10 10 100 100)
        (draw-rect-filled 0 0 200 200 "red")
        (pop-clip-rect)
    )");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 3u);
    EXPECT_EQ(dc.recorded[0].function, "pushClipRect");
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 10.0f);
    EXPECT_EQ(dc.recorded[1].function, "drawRectFilled");
    EXPECT_EQ(dc.recorded[2].function, "popClipRect");
}

// --- Offset ---

TEST_F(JanetCanvasTest, OffsetAppliedDuringRender) {
    dc.offset = {100, 200};
    setScript("(draw-circle-filled 10 20 5 \"red\")");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 110.0f); // 10 + 100
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 220.0f); // 20 + 200
}

// --- Script edge cases ---

TEST_F(JanetCanvasTest, ScriptMultipleDrawCalls) {
    setScript(R"(
        (draw-rect-filled 0 0 200 100 "#333")
        (draw-line 0 50 200 50 "#fff" 2)
        (draw-text 10 10 "#0f0" "JanetCanvas")
    )");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 3u);
    EXPECT_EQ(dc.recorded[0].function, "drawRectFilled");
    EXPECT_EQ(dc.recorded[1].function, "drawLine");
    EXPECT_EQ(dc.recorded[2].function, "drawText");
    EXPECT_EQ(dc.recorded[2].textArg, "JanetCanvas");
}

TEST_F(JanetCanvasTest, ReplaceScript) {
    setScript("(draw-circle-filled 0 0 5 \"red\")");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawCircleFilled");

    setScript("(draw-rect 0 0 100 100 \"blue\")");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRect");
}

TEST_F(JanetCanvasTest, ScriptRuntimeException) {
    EXPECT_TRUE(setScript("(error \"boom\")"));
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

TEST_F(JanetCanvasTest, EmptyScript) {
    EXPECT_TRUE(setScript(""));
    EXPECT_TRUE(hasRenderFunc);
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);
}

TEST_F(JanetCanvasTest, LargeNumberOfDrawCalls) {
    setScript(R"(
        (for i 0 1000
            (draw-line i 0 i 100 "#fff"))
    )");
    callRender();
    EXPECT_EQ(dc.recorded.size(), 1000u);
}

TEST_F(JanetCanvasTest, NestedData) {
    setData("{:position {:x 42 :y 84} :style {:color \"#ff0000\"}}");
    setScript("(draw-circle-filled ((data :position) :x) ((data :position) :y) 5 ((data :style) :color))");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[0], 42.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[1], 84.0f);
}

TEST_F(JanetCanvasTest, ClearThenSetScript) {
    setScript("(draw-circle-filled 0 0 5 \"red\")");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);

    clear();
    callRender();
    EXPECT_EQ(dc.recorded.size(), 0u);

    setScript("(draw-rect 0 0 100 50 \"blue\")");
    callRender();
    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_EQ(dc.recorded[0].function, "drawRect");
}

// --- Canvas dimensions ---

TEST_F(JanetCanvasTest, CanvasDimensions) {
    janetSetVar("canvas-width", janet_wrap_number(400.0));
    janetSetVar("canvas-height", janet_wrap_number(300.0));

    setScript("(draw-rect-filled 0 0 canvas-width canvas-height \"#000\")");
    callRender();

    ASSERT_EQ(dc.recorded.size(), 1u);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[2], 400.0f);
    EXPECT_FLOAT_EQ(dc.recorded[0].floatArgs[3], 300.0f);
}

// --- Janet math ---

TEST_F(JanetCanvasTest, MathAvailable) {
    setScript(R"(
        (def cx 100)
        (def cy 100)
        (def r 50)
        (for i 0 12
            (let [angle (/ (* i math/pi) 6)
                  x (+ cx (* r (math/cos angle)))
                  y (+ cy (* r (math/sin angle)))]
                (draw-circle-filled x y 3 "white")))
    )");
    callRender();
    EXPECT_EQ(dc.recorded.size(), 12u);
}
