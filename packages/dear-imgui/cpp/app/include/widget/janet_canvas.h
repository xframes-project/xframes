#pragma once

extern "C" {
#include <janet.h>
}

#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "styled_widget.h"
#include "canvas_resources.h"
#include "draw_context.h"
#include "texture_helpers.h"

class JanetCanvas final : public StyledWidget {
private:
    friend class XFramesTest;
    JanetCanvas(XFrames* view, int id, std::optional<WidgetStyle>& style, const std::string& bootstrap);
    static int s_janetRefCount;
    bool m_ownsJanetRuntime = false;

    JanetTable* m_env = nullptr;
    Janet m_renderFuncValue = janet_wrap_nil();
    JanetFunction* m_renderFunc = nullptr;
    bool m_hasRenderFunc = false;
    CanvasResources m_resources;
    DrawContext m_drawContext;
    float m_lastCanvasWidth = 0;
    float m_lastCanvasHeight = 0;

    void InitJanet(const std::string& bootstrap);
    void CleanupJanet();
    void SetScriptFromString(const std::string& script);

public:
    static std::unique_ptr<JanetCanvas> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        return std::make_unique<JanetCanvas>(view, id, maybeStyle);
    }

    JanetCanvas(XFrames* view, const int id, std::optional<WidgetStyle>& style);
    ~JanetCanvas();

    void PrepareFrame(XFrames* view) override;
    json GetResourceDiagnostics() const override { auto state = m_resources.Diagnostics(); state["scriptReady"] = m_hasRenderFunc; return state; }
    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
    void Patch(const json& widgetPatchDef, XFrames* view) override;
    bool HasInternalOps() override;
    void HandleInternalOp(const json& opDef) override;

};
