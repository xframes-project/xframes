#include <imgui.h>
#include <nlohmann/json.hpp>
#include <stdexcept>


#include "widget/lua_canvas.h"
#include "sol2_draw_bindings.h"
#include "lua_canvas2d_shim.h"
#include "xframes.h"
#include "imgui_renderer.h"

using json = nlohmann::json;


// Recursively convert nlohmann::json to a sol::object (Lua value)
static sol::object jsonToLua(sol::state& lua, const json& j) {
    if (j.is_null()) return sol::make_object(lua, sol::nil);
    if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
    if (j.is_number_integer()) return sol::make_object(lua, j.get<int64_t>());
    if (j.is_number_float()) return sol::make_object(lua, j.get<double>());
    if (j.is_string()) return sol::make_object(lua, j.get<std::string>());
    if (j.is_array()) {
        sol::table t = lua.create_table();
        for (size_t i = 0; i < j.size(); i++) {
            t[static_cast<int>(i) + 1] = jsonToLua(lua, j[i]); // Lua arrays are 1-indexed
        }
        return t;
    }
    if (j.is_object()) {
        sol::table t = lua.create_table();
        for (auto& [key, val] : j.items()) {
            t[key] = jsonToLua(lua, val);
        }
        return t;
    }
    return sol::make_object(lua, sol::nil);
}

LuaCanvas::LuaCanvas(XFrames* view, const int id, std::optional<WidgetStyle>& style)
    : LuaCanvas(view, id, style, getLuaCanvas2DShim()) {}

LuaCanvas::LuaCanvas(XFrames* view, int id, std::optional<WidgetStyle>& style, const std::string& bootstrap)
    : StyledWidget(view, id, style), m_resources(view) {
    m_type = "di-lua-canvas";
    InitLua(bootstrap); // sol::state unwinds through RAII if initialization fails.
}

LuaCanvas::~LuaCanvas() {
    // sol::state destructor handles Lua cleanup via RAII
}

void LuaCanvas::InitLua(const std::string& bootstrap) {
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    m_drawContext.drawList = nullptr;
    m_drawContext.offset = {0, 0};
    m_drawContext.recording = false;

    Sol2DrawBindings::registerDrawBindings(m_lua, m_drawContext);

    // Set textureLookup once — lambda captures `this` which is stable for widget lifetime
    m_drawContext.textureLookup = [this](const std::string& id) -> ImTextureID { return m_resources.Lookup(id); };

    // Evaluate Canvas 2D API shim — creates global `ctx` table
    auto shimResult = m_lua.safe_script(bootstrap, sol::script_pass_on_error);
    if (!shimResult.valid()) {
        sol::error err = shimResult;
        throw std::runtime_error(err.what());
    }
}

void LuaCanvas::SetScriptFromString(const std::string& script) {
    m_hasRenderFunc = false;

    // Wrap user script in a function so we can call it each frame
    std::string wrapped = "return function() " + script + " end";
    auto result = m_lua.safe_script(wrapped, sol::script_pass_on_error);

    if (!result.valid()) {
        sol::error err = result;
        if (m_view->m_onScriptError) {
            m_view->m_onScriptError(m_id, err.what());
        }
        return;
    }

    m_renderFunc = result.get<sol::protected_function>();
    m_hasRenderFunc = true;
}

void LuaCanvas::PrepareFrame(XFrames* view) {
    m_resources.Prepare([this](const std::string& script) { SetScriptFromString(script); });
}

void LuaCanvas::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    m_resources.SetVisibleScript(m_hasRenderFunc);
    float w = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    float h = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImVec2 pos = ImGui::GetCursorScreenPos();

    m_drawContext.drawList = ImGui::GetWindowDrawList();
    m_drawContext.offset = pos;
    m_drawContext.currentFont = ImGui::GetFont();

    // Only update canvas dimensions when they actually change
    if (w != m_lastCanvasWidth || h != m_lastCanvasHeight) {
        m_lua["canvasWidth"] = w;
        m_lua["canvasHeight"] = h;
        m_lastCanvasWidth = w;
        m_lastCanvasHeight = h;
    }

    if (m_hasRenderFunc) {
        auto result = m_renderFunc();
        if (!result.valid()) {
            sol::error err = result;
            if (view->m_onScriptError) {
                view->m_onScriptError(m_id, err.what());
            }
        }
    }

    ImGui::Dummy(ImVec2(w, h));
}

void LuaCanvas::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);
}

bool LuaCanvas::HasInternalOps() {
    return true;
}

void LuaCanvas::HandleInternalOp(const json& opDef) {
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

        // Convert JSON to Lua table and set as global "data"
        m_lua["data"] = jsonToLua(m_lua, opDef["data"]);
    } else if (op == "clear") {
        m_resources.CancelPendingScript();
        m_hasRenderFunc = false;
        m_lua["data"] = sol::nil;
    }
}
