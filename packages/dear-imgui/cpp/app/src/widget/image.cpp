#include <cstdio>
#include <imgui.h>

#include "widget/image.h"
#include "reactimgui.h"
#include "imgui_renderer.h"


#ifndef __EMSCRIPTEN__
size_t curlCallback(void *data, size_t size, size_t nmemb, void *userp) {
    printf("curlCallback a\n");

    size_t realsize = size * nmemb;

    printf("curlCallback b\n");
    struct memory *mem = (struct memory *)userp;

    printf("curlCallback c\n");

    char *ptr = (char*)realloc(mem->response, mem->size + realsize + 1);

    printf("curlCallback d\n");

    if(!ptr)
        return 0;  /* out of memory */

    printf("curlCallback d\n");

    mem->response = ptr;

    printf("curlCallback e\n");

    memcpy(&(mem->response[mem->size]), data, realsize);

    printf("curlCallback f\n");

    mem->size += realsize;

    printf("curlCallback g\n");

    mem->response[mem->size] = 0;

    printf("curlCallback h\n");

    return realsize;
}
#endif

bool Image::HasCustomWidth() {
    return false;
}

bool Image::HasCustomHeight() {
    return false;
}

void Image::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    // if (m_texture.textureView) {
        auto imageSize = m_size.has_value() ? m_size.value() : ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));



        // if (imageSize.x != 0 && imageSize.y != 0) {
            ImGui::PushID(m_id);
            ImGui::BeginGroup();

            // ImGui::Text("%x", m_view->m_renderer->m_textureId);

            // ImGui::InvisibleButton("##image", imageSize);
            // ImDrawList* drawList = ImGui::GetWindowDrawList();
            //
            // if (!ImGui::IsItemVisible()) {
            //     // Skip rendering as ImDrawList elements are not clipped.
            //     ImGui::EndGroup();
            //     ImGui::PopID();
            //     return;
            // }

            const ImVec2 p0 = ImGui::GetItemRectMin();
            const ImVec2 p1 = ImGui::GetItemRectMax();

        #ifdef __EMSCRIPTEN__
            drawList->AddImage((void*)m_texture.textureView, p0, p1, ImVec2(0, 0), ImVec2(1, 1));
        #else
            // drawList->AddImage((ImTextureID)(intptr_t)view->m_renderer->m_imGuiCtx->IO.Fonts->TexID, p0, p1, ImVec2(0, 0), ImVec2(1, 1));

            ImGui::Image((ImTextureID)(intptr_t)m_view->m_renderer->m_textureId, ImVec2(24, 24));
        #endif

            // ImVec2 uv_min = ImVec2(0.0f, 0.0f);                 // Top-left
            // ImVec2 uv_max = ImVec2(1.0f, 1.0f);                 // Lower-right
            // ImVec4 tint_col = use_text_color_for_tint ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
            // ImVec4 border_col = ImGui::GetStyleColorVec4(ImGuiCol_Border);
            // ImGui::Image(my_tex_id, ImVec2(my_tex_w, my_tex_h), uv_min, uv_max, tint_col, border_col);

            ImGui::EndGroup();
            ImGui::PopID();
        // }
    // }
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
#else
void Image::HandleFetchImageSuccess(void *buffer, size_t realSize) {
    // if (m_view->m_renderer->LoadTexture(buffer, (int)realSize, &m_textureId)) {
    //     printf("Fetched image using url %s and loaded as texture\n", m_url.c_str());
    //
    //     YGNodeMarkDirty(m_layoutNode->m_node);
    // } else {
    //     printf("Fetched image using url %s but unable to load as texture\n", m_url.c_str());
    // }
};

void Image::HandleFetchImageFailure() {
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

void Image::FetchImage() {


    // httplib::Client cli(m_parsedUrl.get_host());

    // auto res = cli.Get(std::string(m_parsedUrl.get_pathname()));

    // printf("%d\n", res->status);

    // HandleFetchImageSuccess((void*)res->body.c_str(), res->body.size());

    // int my_image_width = 0;
    // int my_image_height = 0;
    // GLuint my_image_texture = 0;

    printf("FetchImage()\n");

    // bool ret = m_view->m_renderer->LoadTexture();
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