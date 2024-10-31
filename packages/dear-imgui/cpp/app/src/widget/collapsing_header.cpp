#include <string>

#include "widget/styled_widget.h"
#include "widget/collapsing_header.h"

CollapsingHeader::CollapsingHeader(XFrames* view, const int id, const std::string& label, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "collapsing-header";
    m_handlesChildrenWithinRenderMethod = true;
    m_label = label;
}

void CollapsingHeader::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    YGNodeStyleSetMargin(m_layoutNode->m_node, YGEdgeBottom, m_view->GetFrameHeightWithSpacing(this));

    ImGui::PushID(m_id);
    if (ImGui::CollapsingHeader(m_label.c_str())) {
        SetChildrenDisplay(view, YGDisplayFlex);

        const float left = YGNodeLayoutGetLeft(m_layoutNode->m_node);
        const float top = YGNodeLayoutGetTop(m_layoutNode->m_node);
        const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
        const float height = YGNodeLayoutGetHeight(m_layoutNode->m_node);

        ImGui::SetCursorPos(ImVec2(left, top + m_view->GetFrameHeightWithSpacing(this))); // account for header

        ImGui::BeginChild("##", ImVec2(width, height), ImGuiChildFlags_None);

        ImGui::SetCursorPos(ImVec2(0, 0));

        Widget::HandleChildren(view, viewport);

        ImGui::EndChild();
    } else {
        SetChildrenDisplay(view, YGDisplayNone);
    }
    ImGui::PopID();
};

void CollapsingHeader::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("label") && widgetPatchDef["label"].is_string()) {
        m_label = widgetPatchDef["label"].template get<std::string>();
    }
};

bool CollapsingHeader::HasCustomWidth() {
    return false;
}

bool CollapsingHeader::HasCustomHeight() {
    return false;
}