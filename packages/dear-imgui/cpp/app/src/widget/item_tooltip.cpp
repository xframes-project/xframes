#include <imgui.h>

#include "widget/item_tooltip.h"
#include "widget/widget.h"

// A tooltip is drawn in a window of its own over everything else, so its layout node is taken out
// of the flow of its siblings: they are laid out as if it were not there. Its children keep their
// own layout below it.
ItemTooltip::ItemTooltip(XFrames* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "item-tooltip";
    m_handlesChildrenWithinRenderMethod = true;
    m_layoutNode->TakeOutOfFlow();
}

// ImGui shows it while the item drawn before it is hovered. Its children are placed from where the
// tooltip window starts its contents, inside the window's padding.
void ItemTooltip::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    if (ImGui::BeginItemTooltip()) {
        view->PushLayoutOrigin();
        Widget::HandleChildren(view, viewport);
        view->PopLayoutOrigin();

        ImGui::EndTooltip();
    }
};
