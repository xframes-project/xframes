#include "styled_widget.h"

class TabBar final : public StyledWidget {
public:
    bool m_reorderable = false;

    static std::unique_ptr<TabBar> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();

        auto widget = std::make_unique<TabBar>(view, id, maybeStyle);

        if (widgetDef.contains("reorderable") && widgetDef["reorderable"].is_boolean()) {
            widget->m_reorderable = widgetDef["reorderable"].template get<bool>();
        }

        return widget;
    }

    TabBar(XFrames* view, int id, std::optional<WidgetStyle>& style);

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

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
    bool m_closeable = false;
    bool m_open = true;

    static std::unique_ptr<TabItem> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        auto label = widgetDef["label"].template get<std::string>();

        auto widget = std::make_unique<TabItem>(view, id, label, maybeStyle);

        if (widgetDef.contains("closeable") && widgetDef["closeable"].is_boolean()) {
            widget->m_closeable = widgetDef["closeable"].template get<bool>();
        }

        return widget;
    }

    TabItem(XFrames* view, int id, const std::string& label, std::optional<WidgetStyle>& style);

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;
};