#include <string>

#include "widget/button.h"
#include "widget/styled_widget.h"

std::unique_ptr<Button> Button::makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
    auto id = widgetDef["id"].template get<int>();
    auto label = widgetDef.contains("label") && widgetDef["label"].is_string() ? widgetDef["label"].template get<std::string>() : "";

    return makeWidget(view, id, label, maybeStyle);

    // throw std::invalid_argument("Invalid JSON data");
};

// size is passed as part of rendering the widget
bool Button::HasCustomWidth() {
    return false;
}

bool Button::HasCustomHeight() {
    return false;
}

void Button::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    ImVec2 size = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    if (ImGui::Button(m_label.c_str(), size)) {
        view->m_onClick(m_id);
    }
    ImGui::PopID();
};

void Button::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("label") && widgetPatchDef["label"].is_string()) {
        m_label = widgetPatchDef["label"].template get<std::string>();
    }
};