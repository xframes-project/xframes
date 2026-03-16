#pragma once

extern "C" {
#include <janet.h>
}

#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "styled_widget.h"
#include "draw_context.h"
#include "texture_helpers.h"

class JanetCanvas final : public StyledWidget {
private:
    static int s_janetRefCount;

    JanetTable* m_env = nullptr;
    Janet m_renderFuncValue = janet_wrap_nil();
    JanetFunction* m_renderFunc = nullptr;
    bool m_hasRenderFunc = false;
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
    std::vector<std::string> m_pendingScripts; // guarded by m_textureMutex

#ifdef __EMSCRIPTEN__
    std::unordered_set<std::string> m_inFlightFetches; // guarded by m_textureMutex
#endif

    void InitJanet();
    void SetScriptFromString(const std::string& script);

public:
    static std::unique_ptr<JanetCanvas> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        return std::make_unique<JanetCanvas>(view, id, maybeStyle);
    }

    JanetCanvas(XFrames* view, const int id, std::optional<WidgetStyle>& style);
    ~JanetCanvas();

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
    void Patch(const json& widgetPatchDef, XFrames* view) override;
    bool HasInternalOps() override;
    void HandleInternalOp(const json& opDef) override;

#ifdef __EMSCRIPTEN__
    void EnqueuePendingLoad(std::string textureId, std::vector<unsigned char> data);
    void EnqueuePendingScript(std::string script);
    void ClearInFlightFetch(const std::string& textureId);
#endif
};
