#include "styled_widget.h"

class Separator final : public StyledWidget {
public:
    static std::unique_ptr<Separator> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();

        return std::make_unique<Separator>(view, id, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
    }

    static YGSize Measure(YGNodeConstRef node, const float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
        YGSize size;

        size.width = width;
        // TODO: we likely need to compute this based on the associated font, based on the widget's style
        size.height = 1.0f;

        return size;
    }

    Separator(XFrames* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {}

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Init(const json& elementDef) override {
        Element::Init(elementDef);

        YGNodeSetContext(m_layoutNode->m_node, this);
        YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
    }
};
