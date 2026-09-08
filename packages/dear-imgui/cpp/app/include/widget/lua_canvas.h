#pragma once

#include <sol/sol.hpp>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "styled_widget.h"
#include "canvas_resources.h"
#include "draw_context.h"
#include "texture_helpers.h"

class LuaCanvas final : public StyledWidget {
private:
    friend class XFramesTest;
    LuaCanvas(XFrames* view, int id, std::optional<WidgetStyle>& style, const std::string& bootstrap);
    sol::state m_lua;
    sol::protected_function m_renderFunc;
    bool m_hasRenderFunc = false;
    CanvasResources m_resources;
    DrawContext m_drawContext;
    float m_lastCanvasWidth = 0;
    float m_lastCanvasHeight = 0;

    void InitLua(const std::string& bootstrap);
    void SetScriptFromString(const std::string& script);

public:
    static std::unique_ptr<LuaCanvas> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        return std::make_unique<LuaCanvas>(view, id, maybeStyle);
    }

    LuaCanvas(XFrames* view, const int id, std::optional<WidgetStyle>& style);
    ~LuaCanvas();

    void PrepareFrame(XFrames* view) override;
    json GetResourceDiagnostics() const override { auto state = m_resources.Diagnostics(); state["scriptReady"] = m_hasRenderFunc; return state; }
    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
    void Patch(const json& widgetPatchDef, XFrames* view) override;
    bool HasInternalOps() override;
    void HandleInternalOp(const json& opDef) override;

};
