#include <imgui.h>
#include <nlohmann/json.hpp>

#include "widget/canvas.h"
#include "xframes.h"

Canvas::Canvas(XFrames* view, const int id, std::optional<WidgetStyle>& style)
    : StyledWidget(view, id, style) {
    m_type = "di-canvas";
    InitQuickJS();
}

Canvas::~Canvas() {
    CleanupQuickJS();
}

void Canvas::InitQuickJS() {
    m_runtime = JS_NewRuntime();
    if (!m_runtime) return;

    m_context = JS_NewContext(m_runtime);
    if (!m_context) {
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
        return;
    }

    m_drawContext.drawList = nullptr;
    m_drawContext.offset = {0, 0};
    m_drawContext.recording = false;

    JS_SetContextOpaque(m_context, &m_drawContext);
    QuickJSDrawBindings::registerDrawBindings(m_context);
}

void Canvas::CleanupQuickJS() {
    if (m_context) {
        if (!JS_IsUndefined(m_renderFunc)) {
            JS_FreeValue(m_context, m_renderFunc);
            m_renderFunc = JS_UNDEFINED;
        }
        JS_FreeContext(m_context);
        m_context = nullptr;
    }
    if (m_runtime) {
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
    }
}

void Canvas::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    if (!m_context) return;

    float w = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    float h = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImVec2 pos = ImGui::GetCursorScreenPos();

    m_drawContext.drawList = ImGui::GetWindowDrawList();
    m_drawContext.offset = pos;

    if (!JS_IsUndefined(m_renderFunc) && JS_IsFunction(m_context, m_renderFunc)) {
        JSValue global = JS_GetGlobalObject(m_context);
        JSValue result = JS_Call(m_context, m_renderFunc, global, 0, nullptr);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(m_context);
            JS_FreeValue(m_context, exc);
        }
        JS_FreeValue(m_context, result);
        JS_FreeValue(m_context, global);
    }

    ImGui::Dummy(ImVec2(w, h));
}

void Canvas::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);
}

bool Canvas::HasInternalOps() {
    return true;
}

void Canvas::HandleInternalOp(const json& opDef) {
    if (!m_context || !opDef.contains("op")) return;

    auto op = opDef["op"].template get<std::string>();

    if (op == "setScript") {
        if (!opDef.contains("script")) return;
        auto script = opDef["script"].template get<std::string>();

        // Free previous render function
        if (!JS_IsUndefined(m_renderFunc)) {
            JS_FreeValue(m_context, m_renderFunc);
            m_renderFunc = JS_UNDEFINED;
        }

        // Wrap user script in a function so we can call it each frame
        std::string wrapped = "(function() { " + script + " })";
        JSValue val = JS_Eval(m_context, wrapped.c_str(), wrapped.size(), "<canvas>", JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(val)) {
            JSValue exc = JS_GetException(m_context);
            JS_FreeValue(m_context, exc);
            JS_FreeValue(m_context, val);
            return;
        }

        m_renderFunc = val; // store the function
    } else if (op == "setData") {
        if (!opDef.contains("data")) return;

        // Serialize data to JSON string, then parse in QuickJS as globalThis.data
        std::string dataJson = opDef["data"].dump();
        std::string code = "globalThis.data = " + dataJson + ";";
        JSValue result = JS_Eval(m_context, code.c_str(), code.size(), "<data>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(m_context);
            JS_FreeValue(m_context, exc);
        }
        JS_FreeValue(m_context, result);
    } else if (op == "clear") {
        if (!JS_IsUndefined(m_renderFunc)) {
            JS_FreeValue(m_context, m_renderFunc);
            m_renderFunc = JS_UNDEFINED;
        }

        // Clear globalThis.data
        const char* code = "globalThis.data = undefined;";
        JSValue result = JS_Eval(m_context, code, strlen(code), "<clear>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(m_context, result);
    }
}
