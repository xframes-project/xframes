#include <GLES3/gl3.h>

#include <imgui.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif

#include "widget/image.h"
#include "reactimgui.h"
#include "imgui_renderer.h"

using json = nlohmann::json;

bool Image::HasCustomWidth() {
    return false;
}

bool Image::HasCustomHeight() {
    return false;
}

void Image::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
#ifdef __EMSCRIPTEN__
    bool shouldRender = m_texture.textureView;
#else
    bool shouldRender = view->m_imageToTextureMap.contains(m_id);
#endif

    if (shouldRender) {

        auto imageSize = m_size.has_value() ? m_size.value() : ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

        if (imageSize.x != 0 && imageSize.y != 0) {
            ImGui::PushID(m_id);
            ImGui::BeginGroup();

            // ImGui::Text("%x", view->m_imageToTextureMap[m_id]);

            // ImGui::InvisibleButton("##image", imageSize);
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
            // drawList->AddImage((void*)view->m_imageToTextureMap[m_id], p0, p1, ImVec2(0, 0), ImVec2(1, 1));
            drawList->AddImage((ImTextureID)(intptr_t)view->m_imageToTextureMap[m_id], p0, p1, ImVec2(0, 0), ImVec2(1, 1));
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

// void ReactImgui::RenderMap(int id, double centerX, double centerY, int zoom)
void Image::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        auto op = opDef["op"].template get<std::string>();

        if (op == "reloadImage") {
            FetchImage();
        } else if (op == "loadImage") {
            FetchImage();
        }
    }
};

#ifdef __EMSCRIPTEN__
void Image::HandleFetchImageSuccess(emscripten_fetch_t *fetch) {
    m_view->m_renderer->LoadTexture(fetch->data, fetch->numBytes, &m_texture);

    printf("Fetched image using url %s\n", m_url.c_str());

    YGNodeMarkDirty(m_layoutNode->m_node);
};

void Image::HandleFetchImageFailure(emscripten_fetch_t *fetch) {
    printf("Unable to fetch image using url %s\n", m_url.c_str());
};
#endif

#ifdef __EMSCRIPTEN__
void Image::FetchImage() {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    attr.userData = (void*)this;
    attr.onsuccess = [](emscripten_fetch_t *fetch) {
        if (fetch->userData) {
            auto widget = static_cast<Image*>(fetch->userData);
            widget->HandleFetchImageSuccess(fetch);
        } else {
            printf("Error: fetch->userData is null in onsuccess callback.\n");
        }

        emscripten_fetch_close(fetch); // Free data associated with the fetch.
    };

    attr.onerror = [](emscripten_fetch_t *fetch) {
        if (fetch->userData) {
            auto widget = static_cast<Image*>(fetch->userData);
            widget->HandleFetchImageFailure(fetch);
        } else {
            printf("Error: fetch->userData is null in onerror callback.\n");
        }

        emscripten_fetch_close(fetch); // Also free data on failure.
    };

    emscripten_fetch(&attr, m_url.c_str());
};
#else
void Image::QueueFetchImage() {
    printf("QueueFetchImage\n");

    json op;
    op["id"] = m_id;
    op["op"] = "loadImage";

    auto opString = op.dump();

    m_view->QueueElementInternalOp(m_id, opString);
}

void Image::FetchImage() {
    printf("FetchImage\n");

    FILE* f = fopen(R"(C:\u-blox\gallery\ubx\ulogr\react-imgui\packages\dear-imgui\assets\bitcoin-btc-logo_gqud0f.png)", "rb");
    if (f == NULL) {
        printf("Unable to open file\n");
    }

    fseek(f, 0, SEEK_END);

    size_t file_size = (size_t)ftell(f);

    if (file_size == -1) {
        printf("Unable to determine file size of image\n");
    }

    fseek(f, 0, SEEK_SET);

    void* file_data = IM_ALLOC(file_size);

    fread(file_data, 1, file_size, f);


    m_view->m_renderer->LoadTexture(file_data, file_size);

    IM_FREE(file_data);
}
#endif

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