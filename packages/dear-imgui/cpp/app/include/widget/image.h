#pragma once

#include <optional>
#include <memory>
#include <mutex>
#include "frame_scheduler.h"
#include "wasm_fetches.h"
#include "ada.h"
#include "styled_widget.h"
#include "texture_helpers.h"
#include <nlohmann/json.hpp>

class Image final : public StyledWidget {
private:
    std::string m_url;
    std::optional<ImVec2> m_size;
    Texture m_texture;
    xframes::FrameScheduler::Owner m_resourceLifetime;
    struct CompletionState {
        explicit CompletionState(xframes::FrameScheduler::Source source) : source(std::move(source)) {}
        std::mutex mutex;
        bool alive = true, pending = false, success = false;
        std::vector<unsigned char> data;
        xframes::FrameScheduler::Source source;
    };
    std::shared_ptr<CompletionState> m_completion;
    bool m_loadRequested = false, m_lastLoadFailed = false;
#ifdef __EMSCRIPTEN__
    WasmFetches m_fetches;
#endif
    void RequestImage();

public:
    static std::unique_ptr<Image> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        if (!widgetDef.contains("url") || !widgetDef["url"].is_string()) {
            throw std::invalid_argument("url not defined or not a string");
        }

        auto id = widgetDef["id"].template get<int>();
        auto url = widgetDef["url"].template get<std::string>();

        // URL shape is validated by the shared transaction preflight. Relative
        // asset paths are resolved by the existing backend resource loader.

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

    Image(XFrames* view, int id, const std::string& url, const std::optional<ImVec2>& size, std::optional<WidgetStyle>& style);
    ~Image();
    void PrepareFrame(XFrames* view) override;
    json GetResourceDiagnostics() const override;

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode);

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;

    void Init(const json& elementDef) override {
        Element::Init(elementDef);

        YGNodeSetContext(m_layoutNode->m_node, this);
        YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);

        RequestImage();
    }
};
