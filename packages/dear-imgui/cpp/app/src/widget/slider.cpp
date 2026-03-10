#include <string>

#include "widget/slider.h"
#include "widget/styled_widget.h"

void Slider::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);
    if (m_sliderType == "angle") {
        if (ImGui::SliderAngle("", &m_value, -360.0f, 360.0f, "%.0f")) {
            view->m_onNumericValueChange(m_id, m_value);
        }
    } else {
        if (ImGui::SliderFloat("", &m_value, m_min, m_max, "%.0f")) {
            view->m_onNumericValueChange(m_id, m_value);
        }
    }
    ImGui::PopID();
};

bool Slider::HasInternalOps() {
    return true;
}

void Slider::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        auto op = opDef["op"].template get<std::string>();
        if (op == "setValue" && opDef.contains("value") && opDef["value"].is_number()) {
            m_value = opDef["value"].template get<float>();
        }
    }
}

void Slider::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("min") && widgetPatchDef["min"].is_number()) {
        m_min = widgetPatchDef["min"].template get<float>();
    }
    if (widgetPatchDef.contains("max") && widgetPatchDef["max"].is_number()) {
        m_max = widgetPatchDef["max"].template get<float>();
    }

    if (m_min > m_max) {
        m_min = m_max;
    }
};