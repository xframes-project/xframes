#include "styled_widget.h"

class ProgressBar final : public StyledWidget {
public:
    float m_fraction = 0.0f;
    std::string m_overlay;

    static std::unique_ptr<ProgressBar> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();

        auto fraction = widgetDef.contains("fraction") && widgetDef["fraction"].is_number()
            ? widgetDef["fraction"].template get<float>() : 0.0f;

        std::string overlay;
        if (widgetDef.contains("overlay") && widgetDef["overlay"].is_string()) {
            overlay = widgetDef["overlay"].template get<std::string>();
        }

        return std::make_unique<ProgressBar>(view, id, fraction, overlay, maybeStyle);
    }

    static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
        YGSize size{};
        const auto context = YGNodeGetContext(node);
        if (context) {
            const auto widget = static_cast<ProgressBar*>(context);

            const auto styleWidth = YGNodeStyleGetWidth(node);
            if (styleWidth.unit == YGUnitPoint) {
                size.width = styleWidth.value;
            } else {
                size.width = width;
            }
            size.height = widget->m_view->GetWidgetFontSize(widget) + ImGui::GetStyle().FramePadding.y * 2.0f;
        }

        return size;
    }

    ProgressBar(XFrames* view, const int id, float fraction, const std::string& overlay, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
        m_type = "progress-bar";
        m_fraction = fraction;
        m_overlay = overlay;
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    void Init(const json& elementDef) override {
        Element::Init(elementDef);

        YGNodeSetContext(m_layoutNode->m_node, this);
        YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
    }
};
