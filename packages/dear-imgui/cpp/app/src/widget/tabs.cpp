#include "widget/tabs.h"
#include "xframes.h"

TabBar::TabBar(XFrames* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "tab-bar";
    m_handlesChildrenWithinRenderMethod = true;
}

bool TabBar::HasCustomWidth() {
    return false;
}

bool TabBar::HasCustomHeight() {
    return false;
}

void TabBar::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    const float left = YGNodeLayoutGetLeft(m_layoutNode->m_node);
    const float top = YGNodeLayoutGetTop(m_layoutNode->m_node);
    const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    const float height = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImGui::SetCursorPos(ImVec2(left, top));

    ImGui::BeginChild("##", ImVec2(width, height), ImGuiChildFlags_None);

    ImGuiTabBarFlags flags = ImGuiTabBarFlags_None;
    if (m_reorderable) flags |= ImGuiTabBarFlags_Reorderable;

    if (ImGui::BeginTabBar("", flags)) {
        Widget::HandleChildren(view, viewport);
        ImGui::EndTabBar();
    }

    ImGui::EndChild();
    ImGui::PopID();
};

void TabBar::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("reorderable") && widgetPatchDef["reorderable"].is_boolean()) {
        m_reorderable = widgetPatchDef["reorderable"].template get<bool>();
    }
};

TabItem::TabItem(XFrames* view, const int id, const std::string& label, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "tab-item";
    m_handlesChildrenWithinRenderMethod = true;
    m_label = label;
}

bool TabItem::HasCustomWidth() {
    return false;
}

bool TabItem::HasCustomHeight() {
    return false;
}

void TabItem::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    const bool wasOpen = m_open;
    bool* pOpen = m_closeable ? &m_open : nullptr;

    if (ImGui::BeginTabItem(m_label.c_str(), pOpen)) {
        m_layoutNode->SetDisplay(YGDisplayFlex);

        const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
        const float height = YGNodeLayoutGetHeight(m_layoutNode->m_node);

        if (width > 0 && height > 0) {
            ImGui::SetCursorPos(ImVec2(0, 25.f));
            ImGui::Dummy(ImVec2(0, 0));
            ImGui::BeginChild("##", ImVec2(width, height), ImGuiChildFlags_None);
            Widget::HandleChildren(view, viewport);
            ImGui::EndChild();
        }

        ImGui::EndTabItem();
    } else {
        m_layoutNode->SetDisplay(YGDisplayNone);
    }

    if (m_closeable && wasOpen && !m_open) {
        view->m_onBooleanValueChange(m_id, false);
    }

    ImGui::PopID();
};

void TabItem::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("label") && widgetPatchDef["label"].is_string()) {
        m_label = widgetPatchDef["label"].template get<std::string>();
    }

    if (widgetPatchDef.contains("closeable") && widgetPatchDef["closeable"].is_boolean()) {
        m_closeable = widgetPatchDef["closeable"].template get<bool>();
    }
};