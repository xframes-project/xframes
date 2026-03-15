#pragma once

extern "C" {
#include <quickjs.h>
}

#include <mutex>
#include <unordered_map>

#include "styled_widget.h"
#include "quickjs_draw_bindings.h"
#include "texture_helpers.h"

class Canvas final : public StyledWidget {
private:
    JSRuntime* m_runtime = nullptr;
    JSContext* m_context = nullptr;
    JSValue m_renderFunc = JS_UNDEFINED;
    DrawContext m_drawContext;

    // Texture registry (modified on render thread only)
    std::unordered_map<std::string, Texture> m_textures;

    // Pending texture operations (JS thread -> render thread)
    struct PendingLoad {
        std::string textureId;
        std::vector<unsigned char> fileData;
    };
    struct PendingUnload {
        std::string textureId;
    };
    std::mutex m_textureMutex;
    std::vector<PendingLoad> m_pendingLoads;
    std::vector<PendingUnload> m_pendingUnloads;

    void InitQuickJS();
    void CleanupQuickJS();

public:
    static std::unique_ptr<Canvas> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        return std::make_unique<Canvas>(view, id, maybeStyle);
    }

    Canvas(XFrames* view, const int id, std::optional<WidgetStyle>& style);
    ~Canvas();

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
    void Patch(const json& widgetPatchDef, XFrames* view) override;
    bool HasInternalOps() override;
    void HandleInternalOp(const json& opDef) override;
};
