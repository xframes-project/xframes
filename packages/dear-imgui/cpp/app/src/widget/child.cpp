#include "widget/styled_widget.h"
#include "widget/child.h"

Child::Child(ReactImgui* view, const int id, const float width, const float height, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "child";
    m_handlesChildrenWithinRenderMethod = true;

    m_width = width;
    m_height = height;
};

void Child::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);
    ImGui::BeginChild("", ImVec2(m_width, m_height), m_flags);
    Widget::HandleChildren(view, viewport);
    ImGui::EndChild();
    ImGui::PopID();
};

void Child::Patch(const json& widgetPatchDef, ReactImgui* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef["width"].is_string()) {
        m_width = widgetPatchDef["width"].template get<float>();
    }
    if (widgetPatchDef["height"].is_string()) {
        m_height = widgetPatchDef["height"].template get<float>();
    }
};