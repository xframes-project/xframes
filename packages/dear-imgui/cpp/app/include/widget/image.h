#ifndef WIDGET_IMAGE
#define WIDGET_IMAGE

#include <optional>
#include "ada.h"
// #include "mapgenerator.h"
#include "styled_widget.h"
#include "texture_helpers.h"


#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#else
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#endif

#ifndef __EMSCRIPTEN__
size_t curlCallback(void *data, size_t size, size_t nmemb, void *userp);

struct memory {
    char *response;
    size_t size;
};
#endif

class Image final : public StyledWidget {
private:
    std::string m_url;
    ada::url m_parsedUrl;
    std::optional<ImVec2> m_size;
    Texture m_texture;

#ifndef __EMSCRIPTEN__
    GLuint m_textureId = 0;
#endif

public:
    static std::unique_ptr<Image> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, ReactImgui* view) {
        if (!widgetDef.contains("url") || !widgetDef["url"].is_string()) {
            throw std::invalid_argument("url not defined or not a string");
        }

        auto id = widgetDef["id"].template get<int>();
        auto url = widgetDef["url"].template get<std::string>();

        std::optional<ImVec2> size;

        if (widgetDef.contains("width") && widgetDef.contains("height")) {
            const auto w = widgetDef["width"].template get<float>();
            const auto h = widgetDef["height"].template get<float>();

            size.emplace(ImVec2(w,h));
        }

        return std::make_unique<Image>(view, id, url, size, maybeStyle);
    }

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    Image(ReactImgui* view, const int id, const std::string& url, const std::optional<ImVec2>& size, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style), m_texture() {
        m_type = "di-image";
        m_url = url;

        auto urlParseResult = ada::parse<ada::url>(url);

        if (!urlParseResult) {
            throw std::invalid_argument("Invalid url supplied");
        }

        m_parsedUrl = urlParseResult.value<ada::url>();

        m_size = size;
    }

    void Render(ReactImgui* view, const std::optional<ImRect>& viewport) override;

    static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode);

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;

    void FetchImage();

#ifdef __EMSCRIPTEN__
    void HandleFetchImageSuccess(emscripten_fetch_t *fetch);

    void HandleFetchImageFailure(emscripten_fetch_t *fetch);

#else
    void HandleFetchImageSuccess(void *buffer, size_t realSize);

    void HandleFetchImageFailure();
#endif

    void Init(const json& elementDef) override {
        Element::Init(elementDef);

        YGNodeSetContext(m_layoutNode->m_node, this);
        YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);

        FetchImage();
    }
};

#endif