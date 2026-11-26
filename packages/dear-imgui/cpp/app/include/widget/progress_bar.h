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

    // A bar has a natural width of ten font sizes, like a combo or a text input, so in a row it
    // leaves room for its siblings; a width it is given exactly, as a stretched column item, it
    // fills. Render draws it at the laid-out width and one frame tall, in the widget's own style.
    static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
        YGSize size{};
        const auto context = YGNodeGetContext(node);
        if (context) {
            const auto widget = static_cast<ProgressBar*>(context);

            const float natural = widget->m_view->GetWidgetFontSize(widget) * 10.0f;
            if (widthMode == YGMeasureModeExactly) {
                size.width = width;
            } else if (widthMode == YGMeasureModeAtMost) {
                size.width = std::min(natural, width);
            } else {
                size.width = natural;
            }
            size.height = widget->m_view->GetFrameHeight(widget);
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
