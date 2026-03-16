#include <imgui.h>
#include <nlohmann/json.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#else
#include <fstream>
#include <GLES3/gl3.h>
#endif

#include "widget/janet_canvas.h"
#include "janet_draw_bindings.h"
#include "janet_canvas2d_shim.h"
#include "xframes.h"
#include "imgui_renderer.h"

using json = nlohmann::json;

int JanetCanvas::s_janetRefCount = 0;

#ifdef __EMSCRIPTEN__
struct JanetCanvasFetchContext {
    JanetCanvas* widget;
    std::string textureId;
};

struct JanetScriptFetchContext {
    JanetCanvas* widget;
};
#endif

// Update an existing janet_var binding in-place, or create it if it doesn't exist.
// janet_var creates {:ref @[value]} — we update the ref array's first element directly.
static void janetSetVar(JanetTable* env, const char* name, Janet value) {
    Janet sym = janet_csymbolv(name);
    Janet slot = janet_table_get(env, sym);
    if (janet_checktype(slot, JANET_TABLE)) {
        JanetTable* slotTable = janet_unwrap_table(slot);
        Janet ref = janet_table_get(slotTable, janet_ckeywordv("ref"));
        if (janet_checktype(ref, JANET_ARRAY)) {
            JanetArray* arr = janet_unwrap_array(ref);
            if (arr->count > 0) {
                arr->data[0] = value;
                return;
            }
        }
    }
    // Fallback: create the var binding
    janet_var(env, name, value, NULL);
}

// Recursively convert nlohmann::json to a Janet value
static Janet jsonToJanet(const json& j) {
    if (j.is_null()) return janet_wrap_nil();
    if (j.is_boolean()) return janet_wrap_boolean(j.get<bool>() ? 1 : 0);
    if (j.is_number_integer()) return janet_wrap_number(static_cast<double>(j.get<int64_t>()));
    if (j.is_number_float()) return janet_wrap_number(j.get<double>());
    if (j.is_string()) return janet_cstringv(j.get<std::string>().c_str());
    if (j.is_array()) {
        JanetArray* arr = janet_array(static_cast<int32_t>(j.size()));
        for (size_t i = 0; i < j.size(); i++) {
            janet_array_push(arr, jsonToJanet(j[i]));
        }
        return janet_wrap_array(arr);
    }
    if (j.is_object()) {
        JanetKV* st = janet_struct_begin(static_cast<int32_t>(j.size()));
        for (auto& [key, val] : j.items()) {
            janet_struct_put(st, janet_ckeywordv(key.c_str()), jsonToJanet(val));
        }
        return janet_wrap_struct(janet_struct_end(st));
    }
    return janet_wrap_nil();
}

JanetCanvas::JanetCanvas(XFrames* view, const int id, std::optional<WidgetStyle>& style)
    : StyledWidget(view, id, style) {
    m_type = "di-janet-canvas";
    InitJanet();
}

JanetCanvas::~JanetCanvas() {
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

    if (m_hasRenderFunc) {
        janet_gcunroot(m_renderFuncValue);
    }
    if (m_env) {
        janet_gcunroot(janet_wrap_table(m_env));
        m_env = nullptr;
    }
    s_janetRefCount--;
    if (s_janetRefCount == 0) {
        janet_deinit();
    }
}

void JanetCanvas::InitJanet() {
    if (s_janetRefCount == 0) {
        janet_init();
    }
    s_janetRefCount++;

    // Create a per-widget child env so vars/bindings don't collide
    // (janet_core_env returns the SAME cached table on every call)
    JanetTable* coreEnv = janet_core_env(NULL);
    m_env = janet_table(0);
    m_env->proto = coreEnv;
    janet_gcroot(janet_wrap_table(m_env));

    m_drawContext.drawList = nullptr;
    m_drawContext.offset = {0, 0};
    m_drawContext.recording = false;

    JanetDrawBindings::registerDrawBindings(m_env, m_drawContext);

    // Create mutable var bindings so compiled functions see updates
    janet_var(m_env, "data", janet_wrap_nil(), NULL);
    janet_var(m_env, "canvas-width", janet_wrap_number(0), NULL);
    janet_var(m_env, "canvas-height", janet_wrap_number(0), NULL);

    // Evaluate Canvas 2D API shim — creates global `ctx` table + ctx-xxx functions
    const auto& shim = getJanetCanvas2DShim();
    Janet shimOut;
    int shimStatus = janet_dostring(m_env, shim.c_str(), "canvas2d_shim", &shimOut);
    if (shimStatus != 0) {
        if (m_view->m_onScriptError) {
            m_view->m_onScriptError(m_id, "Janet Canvas 2D shim failed to evaluate");
        }
    }

    // Set textureLookup once — lambda captures `this` which is stable for widget lifetime
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
}

void JanetCanvas::SetScriptFromString(const std::string& script) {
    if (m_hasRenderFunc) {
        janet_gcunroot(m_renderFuncValue);
        m_hasRenderFunc = false;
        m_renderFunc = nullptr;
    }

    // Wrap user script in a function so we can call it each frame
    std::string wrapped = "(fn [] " + script + ")";
    Janet out;
    int status = janet_dostring(m_env, wrapped.c_str(), "script", &out);

    if (status != 0) {
        if (m_view->m_onScriptError) {
            const char* errMsg = janet_checktype(out, JANET_STRING)
                ? (const char*)janet_unwrap_string(out)
                : "Janet compilation error";
            m_view->m_onScriptError(m_id, std::string(errMsg));
        }
        return;
    }

    if (!janet_checktype(out, JANET_FUNCTION)) {
        if (m_view->m_onScriptError) {
            m_view->m_onScriptError(m_id, "Script did not evaluate to a function");
        }
        return;
    }

    m_renderFuncValue = out;
    m_renderFunc = janet_unwrap_function(out);
    janet_gcroot(m_renderFuncValue);
    m_hasRenderFunc = true;
}

void JanetCanvas::Render(XFrames* view, const std::optional<ImRect>& viewport) {
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

    float w = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    float h = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImVec2 pos = ImGui::GetCursorScreenPos();

    m_drawContext.drawList = ImGui::GetWindowDrawList();
    m_drawContext.offset = pos;
    m_drawContext.currentFont = ImGui::GetFont();

    // Point shared static draw context to this widget for this frame
    JanetDrawBindings::s_dc = &m_drawContext;

    // Update canvas dimensions via mutable var bindings
    janetSetVar(m_env, "canvas-width", janet_wrap_number(w));
    janetSetVar(m_env, "canvas-height", janet_wrap_number(h));

    if (m_hasRenderFunc && m_renderFunc) {
        Janet out;
        JanetFiber* fiber = nullptr;
        JanetSignal status = janet_pcall(m_renderFunc, 0, NULL, &out, &fiber);
        if (status != JANET_SIGNAL_OK) {
            if (view->m_onScriptError) {
                const char* errMsg = janet_checktype(out, JANET_STRING)
                    ? (const char*)janet_unwrap_string(out)
                    : "Janet runtime error";
                view->m_onScriptError(m_id, std::string(errMsg));
            }
        }
    }

    ImGui::Dummy(ImVec2(w, h));
}

void JanetCanvas::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);
}

bool JanetCanvas::HasInternalOps() {
    return true;
}

void JanetCanvas::HandleInternalOp(const json& opDef) {
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
        auto* fetchCtx = new JanetScriptFetchContext{this};

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.userData = fetchCtx;

        attr.onsuccess = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<JanetScriptFetchContext*>(fetch->userData);
            std::string script(fetch->data, fetch->numBytes);
            ctx->widget->EnqueuePendingScript(std::move(script));
            delete ctx;
            emscripten_fetch_close(fetch);
        };

        attr.onerror = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<JanetScriptFetchContext*>(fetch->userData);
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

        // Convert JSON to Janet value and update mutable var binding
        Janet janetData = jsonToJanet(opDef["data"]);
        janetSetVar(m_env, "data", janetData);
    } else if (op == "clear") {
        if (m_hasRenderFunc) {
            janet_gcunroot(m_renderFuncValue);
        }
        m_hasRenderFunc = false;
        m_renderFunc = nullptr;
        m_renderFuncValue = janet_wrap_nil();
        janetSetVar(m_env, "data", janet_wrap_nil());
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
        auto* fetchCtx = new JanetCanvasFetchContext{this, textureId};

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.userData = fetchCtx;

        attr.onsuccess = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<JanetCanvasFetchContext*>(fetch->userData);
            std::vector<unsigned char> data(fetch->data, fetch->data + fetch->numBytes);
            ctx->widget->EnqueuePendingLoad(std::move(ctx->textureId), std::move(data));
            delete ctx;
            emscripten_fetch_close(fetch);
        };

        attr.onerror = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<JanetCanvasFetchContext*>(fetch->userData);
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
void JanetCanvas::EnqueuePendingLoad(std::string textureId, std::vector<unsigned char> data) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_inFlightFetches.erase(textureId);
    m_pendingLoads.push_back({std::move(textureId), std::move(data)});
}

void JanetCanvas::EnqueuePendingScript(std::string script) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_pendingScripts.push_back(std::move(script));
}

void JanetCanvas::ClearInFlightFetch(const std::string& textureId) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_inFlightFetches.erase(textureId);
}
#endif
