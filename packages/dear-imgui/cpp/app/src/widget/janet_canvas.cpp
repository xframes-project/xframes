#include <imgui.h>
#include <nlohmann/json.hpp>
#include <stdexcept>


#include "widget/janet_canvas.h"
#include "janet_draw_bindings.h"
#include "janet_canvas2d_shim.h"
#include "xframes.h"
#include "imgui_renderer.h"

using json = nlohmann::json;

int JanetCanvas::s_janetRefCount = 0;


// Update an existing janet_var binding in-place, or create it if it doesn't exist.
// janet_var creates {:ref @[value]} — we update the ref array's first element directly.
static void janetSetVar(JanetTable* env, const char* name, Janet value) {
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
    // Fallback: create the var binding
    janet_var(env, name, value, NULL);
}

// Recursively convert nlohmann::json to a Janet value
static Janet jsonToJanet(const json& j) {
    if (j.is_null()) return janet_wrap_nil();
    if (j.is_boolean()) return janet_wrap_boolean(j.get<bool>() ? 1 : 0);
    if (j.is_number_integer()) return janet_wrap_number(static_cast<double>(j.get<int64_t>()));
    if (j.is_number_float()) return janet_wrap_number(j.get<double>());
    if (j.is_string()) return janet_cstringv(j.get<std::string>().c_str());
    if (j.is_array()) {
        JanetArray* arr = janet_array(static_cast<int32_t>(j.size()));
        for (size_t i = 0; i < j.size(); i++) {
            janet_array_push(arr, jsonToJanet(j[i]));
        }
        return janet_wrap_array(arr);
    }
    if (j.is_object()) {
        JanetKV* st = janet_struct_begin(static_cast<int32_t>(j.size()));
        for (auto& [key, val] : j.items()) {
            janet_struct_put(st, janet_ckeywordv(key.c_str()), jsonToJanet(val));
        }
        return janet_wrap_struct(janet_struct_end(st));
    }
    return janet_wrap_nil();
}

JanetCanvas::JanetCanvas(XFrames* view, const int id, std::optional<WidgetStyle>& style)
    : JanetCanvas(view, id, style, getJanetCanvas2DShim()) {}

JanetCanvas::JanetCanvas(XFrames* view, int id, std::optional<WidgetStyle>& style, const std::string& bootstrap)
    : StyledWidget(view, id, style), m_resources(view) {
    m_type = "di-janet-canvas";
    try { InitJanet(bootstrap); }
    catch (...) { CleanupJanet(); throw; }
}

JanetCanvas::~JanetCanvas() {
    CleanupJanet();
}

void JanetCanvas::CleanupJanet() {
    if (m_hasRenderFunc) {
        janet_gcunroot(m_renderFuncValue);
        m_hasRenderFunc = false;
    }
    if (m_env) {
        janet_gcunroot(janet_wrap_table(m_env));
        m_env = nullptr;
    }
    if (m_ownsJanetRuntime) {
        m_ownsJanetRuntime = false;
        if (--s_janetRefCount == 0) janet_deinit();
    }
}

void JanetCanvas::InitJanet(const std::string& bootstrap) {
    if (s_janetRefCount == 0) {
        janet_init();
    }
    s_janetRefCount++;
    m_ownsJanetRuntime = true;

    // Create a per-widget child env so vars/bindings don't collide
    // (janet_core_env returns the SAME cached table on every call)
    JanetTable* coreEnv = janet_core_env(NULL);
    m_env = janet_table(0);
    m_env->proto = coreEnv;
    janet_gcroot(janet_wrap_table(m_env));

    m_drawContext.drawList = nullptr;
    m_drawContext.offset = {0, 0};
    m_drawContext.recording = false;

    JanetDrawBindings::registerDrawBindings(m_env, m_drawContext);

    // Create mutable var bindings so compiled functions see updates
    janet_var(m_env, "data", janet_wrap_nil(), NULL);
    janet_var(m_env, "canvas-width", janet_wrap_number(0), NULL);
    janet_var(m_env, "canvas-height", janet_wrap_number(0), NULL);

    // Evaluate Canvas 2D API shim — creates global `ctx` table + ctx-xxx functions
    Janet shimOut;
    int shimStatus = janet_dostring(m_env, bootstrap.c_str(), "canvas2d_shim", &shimOut);
    if (shimStatus != 0) {
        throw std::runtime_error("Janet Canvas 2D bootstrap failed to evaluate");
    }

    // Set textureLookup once — lambda captures `this` which is stable for widget lifetime
    m_drawContext.textureLookup = [this](const std::string& id) -> ImTextureID { return m_resources.Lookup(id); };
}

void JanetCanvas::SetScriptFromString(const std::string& script) {
    if (m_hasRenderFunc) {
        janet_gcunroot(m_renderFuncValue);
        m_hasRenderFunc = false;
        m_renderFunc = nullptr;
    }

    // Wrap user script in a function so we can call it each frame
    std::string wrapped = "(fn [] " + script + ")";
    Janet out;
    int status = janet_dostring(m_env, wrapped.c_str(), "script", &out);

    if (status != 0) {
        if (m_view->m_onScriptError) {
            const char* errMsg = janet_checktype(out, JANET_STRING)
                ? (const char*)janet_unwrap_string(out)
                : "Janet compilation error";
            m_view->m_onScriptError(m_id, std::string(errMsg));
        }
        return;
    }

    if (!janet_checktype(out, JANET_FUNCTION)) {
        if (m_view->m_onScriptError) {
            m_view->m_onScriptError(m_id, "Script did not evaluate to a function");
        }
        return;
    }

    m_renderFuncValue = out;
    m_renderFunc = janet_unwrap_function(out);
    janet_gcroot(m_renderFuncValue);
    m_hasRenderFunc = true;
}

void JanetCanvas::PrepareFrame(XFrames* view) {
    m_resources.Prepare([this](const std::string& script) { SetScriptFromString(script); });
}

void JanetCanvas::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    m_resources.SetVisibleScript(m_hasRenderFunc);
    float w = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    float h = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImVec2 pos = ImGui::GetCursorScreenPos();

    m_drawContext.drawList = ImGui::GetWindowDrawList();
    m_drawContext.offset = pos;
    m_drawContext.currentFont = ImGui::GetFont();

    // Point shared static draw context to this widget for this frame
    JanetDrawBindings::s_dc = &m_drawContext;

    // Only update canvas dimensions when they actually change
    if (w != m_lastCanvasWidth || h != m_lastCanvasHeight) {
        janetSetVar(m_env, "canvas-width", janet_wrap_number(w));
        janetSetVar(m_env, "canvas-height", janet_wrap_number(h));
        m_lastCanvasWidth = w;
        m_lastCanvasHeight = h;
    }

    if (m_hasRenderFunc && m_renderFunc) {
        Janet out;
        JanetFiber* fiber = nullptr;
        JanetSignal status = janet_pcall(m_renderFunc, 0, NULL, &out, &fiber);
        if (status != JANET_SIGNAL_OK) {
            if (view->m_onScriptError) {
                const char* errMsg = janet_checktype(out, JANET_STRING)
                    ? (const char*)janet_unwrap_string(out)
                    : "Janet runtime error";
                view->m_onScriptError(m_id, std::string(errMsg));
            }
        }
    }

    ImGui::Dummy(ImVec2(w, h));
}

void JanetCanvas::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);
}

bool JanetCanvas::HasInternalOps() {
    return true;
}

void JanetCanvas::HandleInternalOp(const json& opDef) {
    if (!opDef.contains("op")) return;

    auto op = opDef["op"].template get<std::string>();

    if (m_resources.Handle(opDef)) return;
    if (op == "setScript") {
        m_resources.CancelPendingScript();
        if (!opDef.contains("script")) return;
        auto script = opDef["script"].template get<std::string>();
        SetScriptFromString(script);
    } else if (op == "setData") {
        if (!opDef.contains("data")) return;

        // Convert JSON to Janet value and update mutable var binding
        Janet janetData = jsonToJanet(opDef["data"]);
        janetSetVar(m_env, "data", janetData);
    } else if (op == "clear") {
        m_resources.CancelPendingScript();
        if (m_hasRenderFunc) {
            janet_gcunroot(m_renderFuncValue);
        }
        m_hasRenderFunc = false;
        m_renderFunc = nullptr;
        m_renderFuncValue = janet_wrap_nil();
        janetSetVar(m_env, "data", janet_wrap_nil());
    }
}
