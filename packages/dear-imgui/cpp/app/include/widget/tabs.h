#include "styled_widget.h"

class TabBar final : public StyledWidget {
public:
    static std::unique_ptr<TabBar> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();

        return std::make_unique<TabBar>(view, id, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
    }

    TabBar(XFrames* view, int id, std::optional<WidgetStyle>& style);

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    void Init(const json& elementDef) override {
        Element::Init(elementDef);

        YGNodeStyleSetPadding(m_layoutNode->m_node, YGEdgeTop, 25.0f);
    }
};

class TabItem final : public StyledWidget {
public:
    std::string m_label;

    static std::unique_ptr<TabItem> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        auto label = widgetDef["label"].template get<std::string>();

        return std::make_unique<TabItem>(view, id, label, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
    }

    TabItem(XFrames* view, int id, const std::string& label, std::optional<WidgetStyle>& style);

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;
};