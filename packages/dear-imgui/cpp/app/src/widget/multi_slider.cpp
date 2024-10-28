#include <GLES3/gl3.h>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "widget/multi_slider.h"
#include "reactimgui.h"

void MultiSlider::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    if (m_numValues == 2) {
        if (ImGui::SliderFloat2("", m_values.get(), m_min, m_max, view->m_floatFormatChars[m_decimalDigits].get())) {
            view->m_onMultiValueChange(m_id, m_values.get(), m_numValues);
        }
    } else if (m_numValues == 3) {
        if (ImGui::SliderFloat3("", m_values.get(), m_min, m_max, view->m_floatFormatChars[m_decimalDigits].get())) {
            view->m_onMultiValueChange(m_id, m_values.get(), m_numValues);
        }
    } else if (m_numValues == 4) {
        if (ImGui::SliderFloat4("", m_values.get(), m_min, m_max, view->m_floatFormatChars[m_decimalDigits].get())) {
            view->m_onMultiValueChange(m_id, m_values.get(), m_numValues);
        }
    }

    ImGui::PopID();
};

void MultiSlider::Patch(const json& widgetPatchDef, ReactImgui* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("min") && widgetPatchDef["min"].is_number()) {
        m_min = widgetPatchDef["min"].template get<float>();
    }
    if (widgetPatchDef.contains("max") && widgetPatchDef["max"].is_number()) {
        m_max = widgetPatchDef["max"].template get<float>();
    }
    if (widgetPatchDef.contains("decimalDigits") && widgetPatchDef["decimalDigits"].is_number()) {
        m_decimalDigits = widgetPatchDef["decimalDigits"].template get<int>();
    }
};