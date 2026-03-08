#include <imgui.h>

#include <nlohmann/json.hpp>

#include "widget/color_picker.h"
#include "xframes.h"

void ColorPicker::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);
    ImVec4 color = m_color;
    if (ImGui::ColorEdit4("##", (float*)&color, ImGuiColorEditFlags_NoInputs)) {
        m_color = color;
        auto maybeHexa = IV4toHEXATuple(m_color);
        if (maybeHexa.has_value()) {
            view->m_onInputTextChange(m_id, std::get<0>(maybeHexa.value()));
        }
    }
    ImGui::PopID();
};

bool ColorPicker::HasCustomWidth() {
    return false;
}

bool ColorPicker::HasCustomHeight() {
    return false;
}

void ColorPicker::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("defaultColor")) {
        auto maybeColor = extractColor(widgetPatchDef["defaultColor"]);
        if (maybeColor.has_value()) {
            m_color = maybeColor.value();
        }
    }
};
