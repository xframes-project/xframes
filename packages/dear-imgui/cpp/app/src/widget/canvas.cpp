#include <imgui.h>
#include <nlohmann/json.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#else
#include <fstream>
#include <GLES3/gl3.h>
#endif

#include "widget/canvas.h"
#include "canvas2d_shim.h"
#include "xframes.h"
#include "imgui_renderer.h"

#ifdef __EMSCRIPTEN__
struct CanvasFetchContext {
    Canvas* widget;
    std::string textureId;
};
#endif

Canvas::Canvas(XFrames* view, const int id, std::optional<WidgetStyle>& style)
    : StyledWidget(view, id, style) {
    m_type = "di-canvas";
    InitQuickJS();
}

Canvas::~Canvas() {
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
    CleanupQuickJS();
}

void Canvas::InitQuickJS() {
    m_runtime = JS_NewRuntime();
    if (!m_runtime) return;

#ifdef __EMSCRIPTEN__
    // WASM linear memory confuses QuickJS's C-stack-pointer heuristic,
    // causing immediate "Maximum call stack size exceeded" on JS_Eval.
    // Disable the check — WASM has its own trap-based stack protection.
    JS_SetMaxStackSize(m_runtime, 0);
#endif

    m_context = JS_NewContext(m_runtime);
    if (!m_context) {
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
        return;
    }

    m_drawContext.drawList = nullptr;
    m_drawContext.offset = {0, 0};
    m_drawContext.recording = false;

    JS_SetContextOpaque(m_context, &m_drawContext);
    QuickJSDrawBindings::registerDrawBindings(m_context);

    // Evaluate Canvas 2D API shim — creates globalThis.ctx
    const auto& shim = getCanvas2DShim();
    JSValue shimResult = JS_Eval(m_context, shim.c_str(), shim.size(),
                                 "<canvas2d_shim>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(shimResult)) {
        JSValue exc = JS_GetException(m_context);
        JS_FreeValue(m_context, exc);
    }
    JS_FreeValue(m_context, shimResult);
}

void Canvas::CleanupQuickJS() {
    if (m_context) {
        if (!JS_IsUndefined(m_renderFunc)) {
            JS_FreeValue(m_context, m_renderFunc);
            m_renderFunc = JS_UNDEFINED;
        }
        JS_FreeContext(m_context);
        m_context = nullptr;
    }
    if (m_runtime) {
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
    }
}

void Canvas::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    if (!m_context) return;

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

    // Set canvas dimensions for ctx.canvas.width/height (no string eval, no alloc)
    JSValue global = JS_GetGlobalObject(m_context);
    JS_SetPropertyStr(m_context, global, "__canvasWidth", JS_NewFloat64(m_context, w));
    JS_SetPropertyStr(m_context, global, "__canvasHeight", JS_NewFloat64(m_context, h));
    JS_FreeValue(m_context, global);

    if (!JS_IsUndefined(m_renderFunc) && JS_IsFunction(m_context, m_renderFunc)) {
        JSValue global = JS_GetGlobalObject(m_context);
        JSValue result = JS_Call(m_context, m_renderFunc, global, 0, nullptr);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(m_context);
            JS_FreeValue(m_context, exc);
        }
        JS_FreeValue(m_context, result);
        JS_FreeValue(m_context, global);
    }

    ImGui::Dummy(ImVec2(w, h));
}

void Canvas::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);
}

bool Canvas::HasInternalOps() {
    return true;
}

void Canvas::HandleInternalOp(const json& opDef) {
    if (!m_context || !opDef.contains("op")) return;

    auto op = opDef["op"].template get<std::string>();

    if (op == "setScript") {
        if (!opDef.contains("script")) return;
        auto script = opDef["script"].template get<std::string>();

        // Free previous render function
        if (!JS_IsUndefined(m_renderFunc)) {
            JS_FreeValue(m_context, m_renderFunc);
            m_renderFunc = JS_UNDEFINED;
        }

        // Wrap user script in a function so we can call it each frame
        std::string wrapped = "(function() { " + script + " })";
        JSValue val = JS_Eval(m_context, wrapped.c_str(), wrapped.size(), "<canvas>", JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(val)) {
            JSValue exc = JS_GetException(m_context);
            JS_FreeValue(m_context, exc);
            JS_FreeValue(m_context, val);
            return;
        }

        m_renderFunc = val; // store the function
    } else if (op == "setData") {
        if (!opDef.contains("data")) return;

        // Serialize data to JSON string, then parse in QuickJS as globalThis.data
        std::string dataJson = opDef["data"].dump();
        std::string code = "globalThis.data = " + dataJson + ";";
        JSValue result = JS_Eval(m_context, code.c_str(), code.size(), "<data>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(m_context);
            JS_FreeValue(m_context, exc);
        }
        JS_FreeValue(m_context, result);
    } else if (op == "clear") {
        if (!JS_IsUndefined(m_renderFunc)) {
            JS_FreeValue(m_context, m_renderFunc);
            m_renderFunc = JS_UNDEFINED;
        }

        // Clear globalThis.data
        const char* code = "globalThis.data = undefined;";
        JSValue result = JS_Eval(m_context, code, strlen(code), "<clear>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(m_context, result);
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
        auto* fetchCtx = new CanvasFetchContext{this, textureId};

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.userData = fetchCtx;

        attr.onsuccess = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<CanvasFetchContext*>(fetch->userData);
            std::vector<unsigned char> data(fetch->data, fetch->data + fetch->numBytes);
            ctx->widget->EnqueuePendingLoad(std::move(ctx->textureId), std::move(data));
            delete ctx;
            emscripten_fetch_close(fetch);
        };

        attr.onerror = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<CanvasFetchContext*>(fetch->userData);
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
void Canvas::EnqueuePendingLoad(std::string textureId, std::vector<unsigned char> data) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_inFlightFetches.erase(textureId);
    m_pendingLoads.push_back({std::move(textureId), std::move(data)});
}

void Canvas::ClearInFlightFetch(const std::string& textureId) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_inFlightFetches.erase(textureId);
}
#endif
