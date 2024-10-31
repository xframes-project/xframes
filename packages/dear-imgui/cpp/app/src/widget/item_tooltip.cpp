#include <imgui.h>

#include "widget/item_tooltip.h"
#include "widget/widget.h"

ItemTooltip::ItemTooltip(XFrames* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "item-tooltip";
    m_handlesChildrenWithinRenderMethod = true;
}

void ItemTooltip::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    if (ImGui::BeginItemTooltip()) {
        Widget::HandleChildren(view, viewport);

        ImGui::EndTooltip();
    }
};