#include <GLES3/gl3.h>

#include <imgui.h>

#include "widget/item_tooltip.h"
#include "widget/widget.h"

ItemTooltip::ItemTooltip(ReactImgui* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "item-tooltip";
    m_handlesChildrenWithinRenderMethod = true;
}

void ItemTooltip::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    if (ImGui::BeginItemTooltip()) {
        Widget::HandleChildren(view, viewport);

        ImGui::EndTooltip();
    }
};