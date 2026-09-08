#pragma once

extern "C" {
#include <quickjs.h>
}

#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "styled_widget.h"
#include "canvas_resources.h"
#include "quickjs_draw_bindings.h"
#include "texture_helpers.h"

class JsCanvas final : public StyledWidget {
private:
    friend class XFramesTest;
    JsCanvas(XFrames* view, int id, std::optional<WidgetStyle>& style, const std::string& bootstrap);
    JSRuntime* m_runtime = nullptr;
    JSContext* m_context = nullptr;
    JSValue m_renderFunc = JS_UNDEFINED;
    bool m_hasRenderFunc = false;
    CanvasResources m_resources;
    DrawContext m_drawContext;
    float m_lastCanvasWidth = 0;
    float m_lastCanvasHeight = 0;

    void InitQuickJS(const std::string& bootstrap);
    void CleanupQuickJS();
    void SetScriptFromString(const std::string& script);

public:
    static std::unique_ptr<JsCanvas> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        return std::make_unique<JsCanvas>(view, id, maybeStyle);
    }

    JsCanvas(XFrames* view, const int id, std::optional<WidgetStyle>& style);
    ~JsCanvas();

    void PrepareFrame(XFrames* view) override;
    json GetResourceDiagnostics() const override { auto state = m_resources.Diagnostics(); state["scriptReady"] = m_hasRenderFunc; return state; }
    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
    void Patch(const json& widgetPatchDef, XFrames* view) override;
    bool HasInternalOps() override;
    void HandleInternalOp(const json& opDef) override;

};
