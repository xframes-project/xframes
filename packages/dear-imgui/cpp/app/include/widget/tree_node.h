#include "styled_widget.h"

class TreeNode final : public StyledWidget {
public:
    std::string m_label;
    std::string m_itemId;
    bool m_selectable;
    bool m_leaf;

    std::optional<bool> m_selected;
    std::optional<bool> m_defaultSelected;
    std::optional<bool> m_localSelected;
    std::optional<bool> m_open;
    std::optional<bool> m_defaultOpen;
    std::optional<bool> m_localOpen;

    static std::unique_ptr<TreeNode> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        if (!widgetDef.contains("itemId") || !widgetDef["itemId"].is_string()) {
            throw std::invalid_argument("itemId missing or not a string");
        }

        return std::make_unique<TreeNode>(view, widgetDef["id"].template get<int>(), maybeStyle);
    }

    TreeNode(XFrames* view, int id, std::optional<WidgetStyle>& style);

    void Init(const json& elementDef) override;

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    void SetSelectable(bool selectable);

    void SetSelected(bool selected);

    void SetOpen(bool open);

    void SetLeaf(bool left);

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;
};
