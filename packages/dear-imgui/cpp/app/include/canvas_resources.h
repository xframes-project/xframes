#pragma once

#include "frame_scheduler.h"
#include "texture_helpers.h"
#include "wasm_fetches.h"
#include <imgui.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class XFrames;

// Resource and activity ownership for the three existing Canvas engines.
// Script evaluation stays in each engine. Async callbacks only publish bytes.
class CanvasResources {
public:
    explicit CanvasResources(XFrames* view);
    ~CanvasResources();
    bool Handle(const nlohmann::json& operation);
    void CancelPendingScript();
    void Prepare(const std::function<void(const std::string&)>& evaluate);
    void SetVisibleScript(bool present) { m_activity.Set(present && m_continuous); }
    ImTextureID Lookup(const std::string& id) const;
    nlohmann::json Diagnostics() const;
private:
    struct Load { std::string id; std::vector<unsigned char> bytes; };
    struct Pending {
        explicit Pending(xframes::FrameScheduler::Source source) : source(std::move(source)) {}
        std::mutex mutex;
        bool alive = true, lastLoadFailed = false;
        std::vector<Load> loads;
        std::vector<std::string> scripts;
        std::unordered_set<std::string> fetchingTextures;
        xframes::FrameScheduler::Source source;
    };
    XFrames* m_view;
    xframes::FrameScheduler::Owner m_activity;
    bool m_continuous = true;
    std::shared_ptr<Pending> m_pending;
    std::unordered_map<std::string, Texture> m_textures;
#ifdef __EMSCRIPTEN__
    WasmFetches m_fetches;
#endif
    void LoadTexture(const std::string& id, const std::string& path);
    void UnloadTexture(const std::string& id);
};
