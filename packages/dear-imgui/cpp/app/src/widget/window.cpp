#include <GLES3/gl3.h>

#include <imgui.h>

#include "widget/window.h"
#include "widget/styled_widget.h"

class ReactImgui;

Window::Window(ReactImgui* view, const int id, const std::string& title, const float width, const float height, std::optional<WidgetStyle>& style)
: StyledWidget(view, id, style), m_flags(ImGuiWindowFlags_None), m_open(false), m_title(title), m_width(width), m_height(height) {
    m_type = "di-window";
}

void Window::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    // ImGui::PushID(m_id);
    ImGui::SetNextWindowSize(ImVec2(m_width, m_height), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(m_title.c_str(), &m_open, m_flags)) {
        ImGui::End();
        return;
    }
    Widget::HandleChildren(view, viewport);
    ImGui::End();
    // ImGui::PopID();
};

void Window::Patch(const json& widgetPatchDef, ReactImgui* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef["title"].is_string()) {
        m_title = widgetPatchDef["title"].template get<std::string>();
    }
    if (widgetPatchDef["width"].is_string()) {
        m_width = widgetPatchDef["width"].template get<float>();
    }
    if (widgetPatchDef["height"].is_string()) {
        m_height = widgetPatchDef["height"].template get<float>();
    }
};