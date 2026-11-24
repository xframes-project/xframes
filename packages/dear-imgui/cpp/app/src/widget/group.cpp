#include "widget/group.h"

Group::Group(XFrames* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "group";
    m_handlesChildrenWithinRenderMethod = true;
}

void Group::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::BeginGroup();
    view->PushLayoutOrigin();
    Widget::HandleChildren(view, viewport);
    view->PopLayoutOrigin();
    ImGui::EndGroup();
};