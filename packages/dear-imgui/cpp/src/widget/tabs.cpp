#include "widget/tabs.h"
#include "reactimgui.h"

TabBar::TabBar(ReactImgui* view, const int id, std::optional<BaseStyle>& style) : StyledWidget(view, id, style) {
    m_type = "TabBar";
    m_handlesChildrenWithinRenderMethod = true;
}

bool TabBar::HasCustomWidth() {
    return false;
}

bool TabBar::HasCustomHeight() {
    return false;
}

void TabBar::Render(ReactImgui* view) {
    ImGui::PushID(m_id);
    // todo: double-check if we need to pass a proper id here?

    const float left = YGNodeLayoutGetLeft(m_layoutNode->m_node);
    const float top = YGNodeLayoutGetTop(m_layoutNode->m_node);
    const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    const float height = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImGui::SetCursorPos(ImVec2(left, top));

    ImGui::BeginChild("##", ImVec2(width, height), ImGuiChildFlags_None);

    if (ImGui::BeginTabBar("", ImGuiTabBarFlags_None)) {
        Widget::HandleChildren(view);
        ImGui::EndTabBar();
    }

    ImGui::EndChild();
    ImGui::PopID();
};

TabItem::TabItem(ReactImgui* view, const int id, const std::string& label, std::optional<BaseStyle>& style) : StyledWidget(view, id, style) {
    m_type = "TabItem";
    m_handlesChildrenWithinRenderMethod = true;
    m_label = label;
}

bool TabItem::HasCustomWidth() {
    return false;
}

bool TabItem::HasCustomHeight() {
    return false;
}

void TabItem::Render(ReactImgui* view) {
    ImGui::PushID(m_id);
    if (ImGui::BeginTabItem(m_label.c_str())) {
        m_layoutNode->SetDisplay(YGDisplayFlex);

        const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
        const float height = YGNodeLayoutGetHeight(m_layoutNode->m_node);

        ImGui::SetCursorPos(ImVec2(0, 25.f)); // padding top to account for tabs

        ImGui::BeginChild("##", ImVec2(width, height), ImGuiChildFlags_None);

        Widget::HandleChildren(view);

        ImGui::EndChild();
        ImGui::EndTabItem();
    } else {
        m_layoutNode->SetDisplay(YGDisplayNone);
    }
    ImGui::PopID();
};

void TabItem::Patch(const json& widgetPatchDef, ReactImgui* view) {
    if (widgetPatchDef.is_object()) {
        StyledWidget::Patch(widgetPatchDef, view);

        if (widgetPatchDef.contains("label") && widgetPatchDef["label"].is_string()) {
            m_label = widgetPatchDef["label"].template get<std::string>();
        }
    }
};