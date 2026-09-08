#include <imgui.h>
#include <nlohmann/json.hpp>
#include <stdexcept>


#include "widget/js_canvas.h"
#include "canvas2d_shim.h"
#include "xframes.h"
#include "imgui_renderer.h"


JsCanvas::JsCanvas(XFrames* view, const int id, std::optional<WidgetStyle>& style)
    : JsCanvas(view, id, style, getCanvas2DShim()) {}

JsCanvas::JsCanvas(XFrames* view, int id, std::optional<WidgetStyle>& style, const std::string& bootstrap)
    : StyledWidget(view, id, style), m_resources(view) {
    m_type = "di-js-canvas";
    try { InitQuickJS(bootstrap); }
    catch (...) { CleanupQuickJS(); throw; }
}

JsCanvas::~JsCanvas() {
    CleanupQuickJS();
}

void JsCanvas::InitQuickJS(const std::string& bootstrap) {
    m_runtime = JS_NewRuntime();
    if (!m_runtime) throw std::runtime_error("QuickJS runtime allocation failed");

    // QuickJS's C-stack-pointer heuristic is unreliable in multi-threaded
    // contexts where InitQuickJS and JS_Eval run on different threads.
    JS_SetMaxStackSize(m_runtime, 0);

    m_context = JS_NewContext(m_runtime);
    if (!m_context) {
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
        throw std::runtime_error("QuickJS context allocation failed");
    }

    m_drawContext.drawList = nullptr;
    m_drawContext.offset = {0, 0};
    m_drawContext.recording = false;

    JS_SetContextOpaque(m_context, &m_drawContext);
    QuickJSDrawBindings::registerDrawBindings(m_context);

    // Set textureLookup once — lambda captures `this` which is stable for widget lifetime
    m_drawContext.textureLookup = [this](const std::string& id) -> ImTextureID { return m_resources.Lookup(id); };

    // Evaluate Canvas 2D API shim — creates globalThis.ctx
    JSValue shimResult = JS_Eval(m_context, bootstrap.c_str(), bootstrap.size(),
                                 "<canvas2d_shim>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(shimResult)) {
        JSValue exc = JS_GetException(m_context);
        struct ExceptionDetails {
            JSContext* context;
            JSValue value;
            const char* text;
            ~ExceptionDetails() {
                if (text) JS_FreeCString(context, text);
                JS_FreeValue(context, value);
            }
        } details{m_context, exc, JS_ToCString(m_context, exc)};
        // Bootstrap is framework initialization inside native publication, not
        // an application script event. Never call a user handler under its locks.
        JS_FreeValue(m_context, shimResult);
        throw std::runtime_error(details.text ? details.text : "QuickJS Canvas 2D bootstrap failed");
    }
    JS_FreeValue(m_context, shimResult);
}

void JsCanvas::CleanupQuickJS() {
    m_hasRenderFunc = false;
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

void JsCanvas::SetScriptFromString(const std::string& script) {
    if (!m_context) return;

    m_hasRenderFunc = false;

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
        const char* msg = JS_ToCString(m_context, exc);
        if (msg && m_view->m_onScriptError) {
            m_view->m_onScriptError(m_id, std::string(msg));
        }
        if (msg) JS_FreeCString(m_context, msg);
        JS_FreeValue(m_context, exc);
        JS_FreeValue(m_context, val);
        return;
    }

    m_renderFunc = val;
    m_hasRenderFunc = true;
}

void JsCanvas::PrepareFrame(XFrames* view) {
    m_resources.Prepare([this](const std::string& script) { SetScriptFromString(script); });
}

void JsCanvas::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    m_resources.SetVisibleScript(m_hasRenderFunc);
    float w = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    float h = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImVec2 pos = ImGui::GetCursorScreenPos();

    m_drawContext.drawList = ImGui::GetWindowDrawList();
    m_drawContext.offset = pos;
    m_drawContext.currentFont = ImGui::GetFont();

    // Only update canvas dimensions when they actually change
    if (w != m_lastCanvasWidth || h != m_lastCanvasHeight) {
        JSValue global = JS_GetGlobalObject(m_context);
        JS_SetPropertyStr(m_context, global, "__canvasWidth", JS_NewFloat64(m_context, w));
        JS_SetPropertyStr(m_context, global, "__canvasHeight", JS_NewFloat64(m_context, h));
        JS_FreeValue(m_context, global);
        m_lastCanvasWidth = w;
        m_lastCanvasHeight = h;
    }

    if (m_hasRenderFunc) {
        JSValue global = JS_GetGlobalObject(m_context);
        JSValue result = JS_Call(m_context, m_renderFunc, global, 0, nullptr);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(m_context);
            const char* msg = JS_ToCString(m_context, exc);
            if (msg && view->m_onScriptError) {
                view->m_onScriptError(m_id, std::string(msg));
            }
            if (msg) JS_FreeCString(m_context, msg);
            JS_FreeValue(m_context, exc);
        }
        JS_FreeValue(m_context, result);
        JS_FreeValue(m_context, global);
    }

    ImGui::Dummy(ImVec2(w, h));
}

void JsCanvas::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);
}

bool JsCanvas::HasInternalOps() {
    return true;
}

void JsCanvas::HandleInternalOp(const json& opDef) {
    if (!m_context || !opDef.contains("op")) return;

    auto op = opDef["op"].template get<std::string>();

    if (m_resources.Handle(opDef)) return;
    if (op == "setScript") {
        m_resources.CancelPendingScript();
        if (!opDef.contains("script")) return;
        auto script = opDef["script"].template get<std::string>();
        SetScriptFromString(script);
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
        m_resources.CancelPendingScript();
        m_hasRenderFunc = false;
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
