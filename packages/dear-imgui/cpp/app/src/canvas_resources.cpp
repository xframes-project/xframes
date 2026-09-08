#include "canvas_resources.h"
#include "xframes.h"
#include "imgui_renderer.h"
#include <algorithm>
#include <fstream>
#include <limits>

using xframes::FrameReason;
using json = nlohmann::json;

CanvasResources::CanvasResources(XFrames* view) : m_view(view),
    m_activity(view->m_frameScheduler.Register(FrameReason::Canvas)),
    m_pending(std::make_shared<Pending>(m_activity.GetSource())) {}

CanvasResources::~CanvasResources() {
#ifdef __EMSCRIPTEN__
    m_fetches.CancelAll();
#endif
    {
        const std::lock_guard lock(m_pending->mutex);
        m_pending->alive = false;
        m_pending->loads.clear(); m_pending->scripts.clear(); m_pending->fetchingTextures.clear();
        m_activity.Reset();
    }
    if (m_view->m_renderer)
        for (const auto& [id, texture] : m_textures) m_view->m_renderer->RetireTexture(texture);
}

void CanvasResources::CancelPendingScript() {
#ifdef __EMSCRIPTEN__
    m_fetches.Cancel("script");
#endif
    const std::lock_guard lock(m_pending->mutex);
    m_pending->scripts.clear();
}

bool CanvasResources::Handle(const json& operation) {
    const auto op = operation.at("op").get<std::string>();
    if (op == "setContinuous") {
        m_continuous = operation.at("continuous").get<bool>();
        if (!m_continuous) m_activity.Set(false);
    } else if (op == "redraw") {
        // The owning imperative dispatch invalidates this explicit frame.
    } else if (op == "setScriptFile") {
        CancelPendingScript();
        const auto path = operation.at("path").get<std::string>();
#ifdef __EMSCRIPTEN__
        const std::weak_ptr<Pending> pending = m_pending;
        m_fetches.Get("script", path, [pending](bool success, WasmFetches::Bytes bytes) {
            if (const auto state = pending.lock()) {
                {
                    const std::lock_guard lock(state->mutex);
                    if (!state->alive) return;
                    if (success) state->scripts.emplace_back(bytes.begin(), bytes.end());
                    state->lastLoadFailed = !success;
                    state->source.Invalidate(FrameReason::Resource);
                }
                state->source.Notify();
            }
        });
#else
        std::ifstream file(path);
        const std::lock_guard lock(m_pending->mutex);
        m_pending->lastLoadFailed = !file.is_open();
        if (file) m_pending->scripts.emplace_back(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
#endif
    } else if (op == "loadTexture" || op == "reloadTexture") {
        const auto id = operation.at("textureId").get<std::string>();
        if (op == "reloadTexture") UnloadTexture(id);
        LoadTexture(id, operation.at("source").get<std::string>());
    } else if (op == "unloadTexture") {
        UnloadTexture(operation.at("textureId").get<std::string>());
    } else return false;
    return true;
}

void CanvasResources::UnloadTexture(const std::string& id) {
#ifdef __EMSCRIPTEN__
    m_fetches.Cancel("texture:" + id);
#endif
    {
        const std::lock_guard lock(m_pending->mutex);
        m_pending->fetchingTextures.erase(id);
        std::erase_if(m_pending->loads, [&](const Load& load) { return load.id == id; });
    }
    if (auto it = m_textures.find(id); it != m_textures.end()) {
        m_view->m_renderer->RetireTexture(it->second);
        m_textures.erase(it);
    }
}

void CanvasResources::LoadTexture(const std::string& id, const std::string& path) {
    if (m_textures.contains(id)) return;
    {
        const std::lock_guard lock(m_pending->mutex);
        if (m_pending->fetchingTextures.contains(id)
            || std::any_of(m_pending->loads.begin(), m_pending->loads.end(), [&](const Load& load) { return load.id == id; })) return;
#ifdef __EMSCRIPTEN__
        m_pending->fetchingTextures.insert(id);
#endif
    }
#ifdef __EMSCRIPTEN__
    const std::weak_ptr<Pending> pending = m_pending;
    m_fetches.Get("texture:" + id, path, [pending, id](bool success, WasmFetches::Bytes bytes) {
        if (const auto state = pending.lock()) {
            {
                const std::lock_guard lock(state->mutex);
                if (!state->alive) return;
                state->fetchingTextures.erase(id);
                if (success) state->loads.push_back({id, std::move(bytes)});
                state->lastLoadFailed = !success;
                state->source.Invalidate(FrameReason::Resource);
            }
            state->source.Notify();
        }
    });
#else
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file ? file.tellg() : std::ifstream::pos_type(-1);
    std::vector<unsigned char> bytes;
    if (size > 0 && size <= std::numeric_limits<int>::max()) {
        bytes.resize(static_cast<size_t>(size));
        file.seekg(0); file.read(reinterpret_cast<char*>(bytes.data()), size);
    }
    const std::lock_guard lock(m_pending->mutex);
    m_pending->lastLoadFailed = !file || bytes.empty();
    if (!m_pending->lastLoadFailed) m_pending->loads.push_back({id, std::move(bytes)});
#endif
}

void CanvasResources::Prepare(const std::function<void(const std::string&)>& evaluate) {
    m_activity.Set(false);
    std::vector<Load> loads;
    std::vector<std::string> scripts;
    {
        const std::lock_guard lock(m_pending->mutex);
        loads.swap(m_pending->loads); scripts.swap(m_pending->scripts);
    }
    for (const auto& load : loads) {
        if (m_textures.contains(load.id)) continue;
        Texture texture;
        bool loaded = false;
        if (load.bytes.size() <= static_cast<size_t>(std::numeric_limits<int>::max())) {
#ifdef __EMSCRIPTEN__
            loaded = m_view->m_renderer->LoadTexture(load.bytes.data(), static_cast<int>(load.bytes.size()), &texture);
#else
            texture.textureView = m_view->m_renderer->LoadTexture(load.bytes.data(), static_cast<int>(load.bytes.size()));
            loaded = texture.textureView != 0;
#endif
        }
        if (loaded) m_textures.emplace(load.id, texture);
        const std::lock_guard lock(m_pending->mutex);
        m_pending->lastLoadFailed = !loaded;
    }
    for (const auto& script : scripts) evaluate(script);
}

ImTextureID CanvasResources::Lookup(const std::string& id) const {
    const auto it = m_textures.find(id);
    if (it == m_textures.end()) return 0;
#ifdef __EMSCRIPTEN__
    return (ImTextureID)it->second.textureView;
#else
    return (ImTextureID)(intptr_t)it->second.textureView;
#endif
}

json CanvasResources::Diagnostics() const {
    const std::lock_guard lock(m_pending->mutex);
    return {{"loadedTextures", m_textures.size()}, {"queuedLoads", m_pending->loads.size() + m_pending->scripts.size()},
        {"lastLoadFailed", m_pending->lastLoadFailed}, {"continuous", m_continuous},
#ifdef __EMSCRIPTEN__
        {"pendingRequests", m_fetches.Size()}
#else
        {"pendingRequests", 0}
#endif
    };
}
