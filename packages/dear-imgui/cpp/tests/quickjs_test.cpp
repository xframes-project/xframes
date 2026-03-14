#include <gtest/gtest.h>

extern "C" {
#include <quickjs.h>
}

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
