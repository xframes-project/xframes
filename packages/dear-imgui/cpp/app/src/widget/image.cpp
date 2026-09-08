#include <imgui.h>
#include <limits>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif

#include "widget/image.h"
#include "xframes.h"
#include "imgui_renderer.h"

using json = nlohmann::json;

bool Image::HasCustomWidth() {
    return false;
}

bool Image::HasCustomHeight() {
    return false;
}

Image::Image(XFrames* view, int id, const std::string& url, const std::optional<ImVec2>& size, std::optional<WidgetStyle>& style)
    : StyledWidget(view, id, style), m_url(url), m_size(size),
      m_resourceLifetime(view->m_frameScheduler.Register(xframes::FrameReason::Resource)),
      m_completion(std::make_shared<CompletionState>(m_resourceLifetime.GetSource())) {
    m_type = "di-image";
}
Image::~Image() {
#ifdef __EMSCRIPTEN__
    m_fetches.CancelAll();
#endif
    {
        const std::lock_guard lock(m_completion->mutex);
        m_completion->alive = false;
        m_completion->data.clear();
        m_resourceLifetime.Reset();
    }
    if (m_view->m_renderer) m_view->m_renderer->RetireTexture(m_texture);
}
void Image::PrepareFrame(XFrames* view) {
    Texture loaded;
    bool success = false;
#ifdef __EMSCRIPTEN__
    std::vector<unsigned char> data;
    {
        const std::lock_guard lock(m_completion->mutex);
        if (!m_completion->pending) return;
        m_completion->pending = false;
        success = m_completion->success;
        data.swap(m_completion->data);
    }
    success = success && data.size() <= static_cast<size_t>(std::numeric_limits<int>::max())
        && view->m_renderer->LoadTexture(data.data(), static_cast<int>(data.size()), &loaded);
#else
    if (!m_loadRequested) return;
    m_loadRequested = false;
    success = view->m_renderer->LoadTextureFile(m_url, &loaded);
#endif
    m_lastLoadFailed = !success;
    if (success) {
        view->m_renderer->RetireTexture(m_texture);
        m_texture = loaded;
        YGNodeMarkDirty(m_layoutNode->m_node);
    }
}

json Image::GetResourceDiagnostics() const {
    const std::lock_guard lock(m_completion->mutex);
    return {{"loadedTextures", m_texture.textureView ? 1 : 0},
        {"queuedLoads", (m_loadRequested ? 1 : 0) + (m_completion->pending ? 1 : 0)},
        {"lastLoadFailed", m_lastLoadFailed},
#ifdef __EMSCRIPTEN__
        {"pendingRequests", m_fetches.Size()}
#else
        {"pendingRequests", 0}
#endif
    };
}

void Image::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    const bool shouldRender = m_texture.textureView != 0;
    const auto imageSize = m_size.value_or(ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node)));
    if (!shouldRender || !(imageSize.x > 0 && imageSize.y > 0)) {
        // StyledWidget positioned the cursor for this Yoga box. Even a pending
        // or failed image must submit its layout item before the window ends.
        ImGui::Dummy(ImVec2(std::max(0.0f, imageSize.x), std::max(0.0f, imageSize.y)));
        return;
    }

    if (shouldRender) {

        auto imageSize = m_size.has_value() ? m_size.value() : ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

        if (imageSize.x != 0 && imageSize.y != 0) {
            ImGui::PushID(m_id);
            ImGui::BeginGroup();


             ImGui::InvisibleButton("##image", imageSize);
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            if (!ImGui::IsItemVisible()) {
                // Skip rendering as ImDrawList elements are not clipped.
                ImGui::EndGroup();
                ImGui::PopID();
                return;
            }

            const ImVec2 p0 = ImGui::GetItemRectMin();
            const ImVec2 p1 = ImGui::GetItemRectMax();

        #ifdef __EMSCRIPTEN__
            drawList->AddImage((void*)m_texture.textureView, p0, p1, ImVec2(0, 0), ImVec2(1, 1));
        #else
            drawList->AddImage((ImTextureID)(intptr_t)m_texture.textureView, p0, p1, ImVec2(0, 0), ImVec2(1, 1));

        #endif
            // ImVec2 uv_min = ImVec2(0.0f, 0.0f);                 // Top-left
            // ImVec2 uv_max = ImVec2(1.0f, 1.0f);                 // Lower-right
            // ImVec4 tint_col = use_text_color_for_tint ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
            // ImVec4 border_col = ImGui::GetStyleColorVec4(ImGuiCol_Border);
            // ImGui::Image(my_tex_id, ImVec2(my_tex_w, my_tex_h), uv_min, uv_max, tint_col, border_col);

            ImGui::EndGroup();
            ImGui::PopID();
        }
    }
};

bool Image::HasInternalOps() {
    return true;
}

// void XFrames::RenderMap(int id, double centerX, double centerY, int zoom)
void Image::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        auto op = opDef["op"].template get<std::string>();

        if (op == "reloadImage") {
            RequestImage();
        }
    }
};

void Image::RequestImage() {
#ifdef __EMSCRIPTEN__
    const std::weak_ptr<CompletionState> completion = m_completion;
    m_fetches.Get("image", m_url, [completion](bool success, WasmFetches::Bytes data) {
        if (const auto state = completion.lock()) {
            {
                const std::lock_guard lock(state->mutex);
                if (!state->alive) return;
                state->data = std::move(data);
                state->success = success;
                state->pending = true;
                state->source.Invalidate(xframes::FrameReason::Resource);
            }
            state->source.Notify();
        }
    });
#else
    // Widget ownership replaces the old reusable-ID global image-job queue.
    // Publication/imperative dispatch invalidates after publishing this work.
    m_loadRequested = true;
#endif
}

YGSize Image::Measure(const YGNodeConstRef node, const float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
    YGSize size{};
    const auto context = YGNodeGetContext(node);
    if (context) {
        const auto widget = static_cast<Image*>(context);

        // if (widget->m_size.has_value()) {
        //     size.width = widget->m_size.value().x;
        //     size.height = widget->m_size.value().y;
        // } else {
        //     size.width = static_cast<float>(widget->m_texture.width);
        //     size.height = static_cast<float>(widget->m_texture.height);
        // }

        if (widget->m_size.has_value()) {
            size.width = widget->m_size.value().x;
            size.height = widget->m_size.value().y;
        } else {
            size.width = width;
            size.height = height;
        }
    }

    return size;
};
