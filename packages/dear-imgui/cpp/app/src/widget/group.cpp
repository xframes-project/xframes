#include "widget/group.h"

Group::Group(ReactImgui* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "group";
    m_handlesChildrenWithinRenderMethod = true;
}

void Group::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImGui::BeginGroup();
    Widget::HandleChildren(view, viewport);
    ImGui::EndGroup();
};