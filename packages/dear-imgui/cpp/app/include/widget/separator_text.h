#include "styled_widget.h"

class SeparatorText final : public StyledWidget {
public:
    std::string m_label;

    static std::unique_ptr<SeparatorText> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        if (!widgetDef.contains("label") || !widgetDef["label"].is_string()) {
            throw std::invalid_argument("Missing label or not a string");
        }

        auto id = widgetDef["id"].template get<int>();
        auto label = widgetDef["label"].template get<std::string>();

        return std::make_unique<SeparatorText>(view, id, label, maybeStyle);
    }

    static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
        YGSize size{};
        const auto context = YGNodeGetContext(node);
        if (context) {
            const auto widget = static_cast<SeparatorText*>(context);

            size.width = width; // TODO: sure about that?
            size.height = widget->m_view->GetWidgetFontSize(widget);
        }

        return size;
    }

    SeparatorText(XFrames* view, const int id, const std::string& label, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
        m_type = "separator-text";
        m_label = label;
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    void Init(const json& elementDef) override {
        Element::Init(elementDef);

        YGNodeSetContext(m_layoutNode->m_node, this);
        YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
    }
};
