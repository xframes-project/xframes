#pragma once

#include <sol/sol.hpp>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "styled_widget.h"
#include "draw_context.h"
#include "texture_helpers.h"

class LuaCanvas final : public StyledWidget {
private:
    sol::state m_lua;
    sol::protected_function m_renderFunc;
    bool m_hasRenderFunc = false;
    DrawContext m_drawContext;
    float m_lastCanvasWidth = 0;
    float m_lastCanvasHeight = 0;

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

    void InitLua();
    void SetScriptFromString(const std::string& script);

public:
    static std::unique_ptr<LuaCanvas> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        return std::make_unique<LuaCanvas>(view, id, maybeStyle);
    }

    LuaCanvas(XFrames* view, const int id, std::optional<WidgetStyle>& style);
    ~LuaCanvas();

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
