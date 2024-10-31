#include <imgui.h>

#include <nlohmann/json.hpp>

#include "widget/checkbox.h"
#include "xframes.h"

void Checkbox::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);
    if (ImGui::Checkbox(m_label.c_str(), &m_checked)) {
        view->m_onBooleanValueChange(m_id, m_checked);
    }
    ImGui::PopID();
};

void Checkbox::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("label") && widgetPatchDef["label"].is_string()) {
        m_label = widgetPatchDef["label"].template get<std::string>();
    }
};