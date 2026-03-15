#include <imgui.h>
#include <nlohmann/json.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#else
#include <fstream>
#include <GLES3/gl3.h>
#endif

#include "widget/lua_canvas.h"
#include "sol2_draw_bindings.h"
#include "lua_canvas2d_shim.h"
#include "xframes.h"
#include "imgui_renderer.h"

using json = nlohmann::json;

#ifdef __EMSCRIPTEN__
struct LuaCanvasFetchContext {
    LuaCanvas* widget;
    std::string textureId;
};

struct LuaScriptFetchContext {
    LuaCanvas* widget;
};
#endif

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
    : StyledWidget(view, id, style) {
    m_type = "di-lua-canvas";
    InitLua();
}

LuaCanvas::~LuaCanvas() {
    for (auto& [id, tex] : m_textures) {
        if (tex.textureView) {
#ifdef __EMSCRIPTEN__
            wgpuTextureViewRelease(tex.textureView);
#else
            glDeleteTextures(1, &tex.textureView);
#endif
        }
    }
    m_textures.clear();
    // sol::state destructor handles Lua cleanup via RAII
}

void LuaCanvas::InitLua() {
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    m_drawContext.drawList = nullptr;
    m_drawContext.offset = {0, 0};
    m_drawContext.recording = false;

    Sol2DrawBindings::registerDrawBindings(m_lua, m_drawContext);

    // Evaluate Canvas 2D API shim — creates global `ctx` table
    const auto& shim = getLuaCanvas2DShim();
    auto shimResult = m_lua.safe_script(shim, sol::script_pass_on_error);
    if (!shimResult.valid()) {
        sol::error err = shimResult;
        if (m_view->m_onScriptError) {
            m_view->m_onScriptError(m_id, std::string(err.what()));
        }
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

void LuaCanvas::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    // Process pending texture operations on the render/GL thread
    {
        std::lock_guard<std::mutex> lock(m_textureMutex);

        // Process loads first
        for (auto& pending : m_pendingLoads) {
            if (m_textures.contains(pending.textureId)) continue;

#ifdef __EMSCRIPTEN__
            Texture tex;
            if (view->m_renderer->LoadTexture(
                    pending.fileData.data(),
                    static_cast<int>(pending.fileData.size()),
                    &tex)) {
                m_textures[pending.textureId] = tex;
            }
#else
            GLuint texId = view->m_renderer->LoadTexture(
                pending.fileData.data(),
                static_cast<int>(pending.fileData.size())
            );

            if (texId != 0) {
                Texture tex;
                tex.textureView = texId;
                tex.width = 0;
                tex.height = 0;
                m_textures[pending.textureId] = tex;
            }
#endif
        }
        m_pendingLoads.clear();

        // Process unloads second (load+unload same frame = no texture)
        for (auto& pending : m_pendingUnloads) {
            auto it = m_textures.find(pending.textureId);
            if (it != m_textures.end()) {
                if (it->second.textureView) {
#ifdef __EMSCRIPTEN__
                    wgpuTextureViewRelease(it->second.textureView);
#else
                    glDeleteTextures(1, &it->second.textureView);
#endif
                }
                m_textures.erase(it);
            }
        }
        m_pendingUnloads.clear();

        // Process pending scripts (from WASM async fetch)
        for (auto& script : m_pendingScripts) {
            SetScriptFromString(script);
        }
        m_pendingScripts.clear();
    }

    // Set textureLookup for this frame
    m_drawContext.textureLookup = [this](const std::string& id) -> ImTextureID {
        auto it = m_textures.find(id);
        if (it != m_textures.end() && it->second.textureView) {
#ifdef __EMSCRIPTEN__
            return (ImTextureID)it->second.textureView;
#else
            return (ImTextureID)(intptr_t)it->second.textureView;
#endif
        }
        return 0;
    };

    float w = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    float h = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImVec2 pos = ImGui::GetCursorScreenPos();

    m_drawContext.drawList = ImGui::GetWindowDrawList();
    m_drawContext.offset = pos;
    m_drawContext.currentFont = ImGui::GetFont();

    // Set canvas dimensions as Lua globals
    m_lua["canvasWidth"] = w;
    m_lua["canvasHeight"] = h;

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

    if (op == "setScript") {
        if (!opDef.contains("script")) return;
        auto script = opDef["script"].template get<std::string>();
        SetScriptFromString(script);
    } else if (op == "setScriptFile") {
        if (!opDef.contains("path")) return;
        auto path = opDef["path"].template get<std::string>();

#ifdef __EMSCRIPTEN__
        // Async fetch — callback queues script for evaluation in Render()
        auto* fetchCtx = new LuaScriptFetchContext{this};

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.userData = fetchCtx;

        attr.onsuccess = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<LuaScriptFetchContext*>(fetch->userData);
            std::string script(fetch->data, fetch->numBytes);
            ctx->widget->EnqueuePendingScript(std::move(script));
            delete ctx;
            emscripten_fetch_close(fetch);
        };

        attr.onerror = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<LuaScriptFetchContext*>(fetch->userData);
            delete ctx;
            emscripten_fetch_close(fetch);
        };

        emscripten_fetch(&attr, path.c_str());
#else
        std::ifstream file(path);
        if (!file.is_open()) return;

        std::string script((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        SetScriptFromString(script);
#endif
    } else if (op == "setData") {
        if (!opDef.contains("data")) return;

        // Convert JSON to Lua table and set as global "data"
        m_lua["data"] = jsonToLua(m_lua, opDef["data"]);
    } else if (op == "clear") {
        m_hasRenderFunc = false;
        m_lua["data"] = sol::nil;
    } else if (op == "loadTexture") {
        if (!opDef.contains("textureId") || !opDef.contains("source")) return;

        auto textureId = opDef["textureId"].template get<std::string>();
        auto source = opDef["source"].template get<std::string>();

        // Skip if already loaded or in-flight
        {
            std::lock_guard<std::mutex> lock(m_textureMutex);
            if (m_textures.contains(textureId)) return;
#ifdef __EMSCRIPTEN__
            if (m_inFlightFetches.contains(textureId)) return;
            m_inFlightFetches.insert(textureId);
#endif
        }

#ifdef __EMSCRIPTEN__
        // Async fetch — callback pushes data into pending queue
        auto* fetchCtx = new LuaCanvasFetchContext{this, textureId};

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.userData = fetchCtx;

        attr.onsuccess = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<LuaCanvasFetchContext*>(fetch->userData);
            std::vector<unsigned char> data(fetch->data, fetch->data + fetch->numBytes);
            ctx->widget->EnqueuePendingLoad(std::move(ctx->textureId), std::move(data));
            delete ctx;
            emscripten_fetch_close(fetch);
        };

        attr.onerror = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<LuaCanvasFetchContext*>(fetch->userData);
            ctx->widget->ClearInFlightFetch(ctx->textureId);
            delete ctx;
            emscripten_fetch_close(fetch);
        };

        emscripten_fetch(&attr, source.c_str());
#else
        // Read file on JS thread, queue bytes for GPU upload on render thread
        std::ifstream file(source, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return;

        auto fileSize = file.tellg();
        if (fileSize <= 0) return;

        std::vector<unsigned char> fileData(static_cast<size_t>(fileSize));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
        if (!file) return;

        {
            std::lock_guard<std::mutex> lock(m_textureMutex);
            m_pendingLoads.push_back({std::move(textureId), std::move(fileData)});
        }
#endif
    } else if (op == "unloadTexture") {
        if (!opDef.contains("textureId")) return;

        auto textureId = opDef["textureId"].template get<std::string>();

        {
            std::lock_guard<std::mutex> lock(m_textureMutex);
            m_pendingUnloads.push_back({std::move(textureId)});
        }
    } else if (op == "reloadTexture") {
        if (!opDef.contains("textureId") || !opDef.contains("source")) return;

        auto textureId = opDef["textureId"].template get<std::string>();
        auto source = opDef["source"].template get<std::string>();

        // Queue unload + clear state so the loadTexture guard passes
        {
            std::lock_guard<std::mutex> lock(m_textureMutex);
            m_pendingUnloads.push_back({textureId});
            m_textures.erase(textureId);
#ifdef __EMSCRIPTEN__
            m_inFlightFetches.erase(textureId);
#endif
        }

        // Re-use existing loadTexture logic
        json loadOp = {{"op", "loadTexture"}, {"textureId", textureId}, {"source", source}};
        HandleInternalOp(loadOp);
    }
}

#ifdef __EMSCRIPTEN__
void LuaCanvas::EnqueuePendingLoad(std::string textureId, std::vector<unsigned char> data) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_inFlightFetches.erase(textureId);
    m_pendingLoads.push_back({std::move(textureId), std::move(data)});
}

void LuaCanvas::EnqueuePendingScript(std::string script) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_pendingScripts.push_back(std::move(script));
}

void LuaCanvas::ClearInFlightFetch(const std::string& textureId) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_inFlightFetches.erase(textureId);
}
#endif
