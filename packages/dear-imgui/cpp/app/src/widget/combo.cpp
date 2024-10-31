#include <imgui.h>

#include "widget/combo.h"
#include "xframes.h"

void Combo::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    if (ImGui::BeginCombo("",
        m_selectedIndex >= 0 && m_selectedIndex < m_options.size()
            ? m_options[m_selectedIndex].c_str()
            : m_placeholder.c_str()
        , ImGuiComboFlags_None)) {
        const auto optionsLength = m_options.size();
        for (int n = 0; n < optionsLength; n++)
        {
            const bool is_selected = (m_selectedIndex == n);
            if (ImGui::Selectable(m_options[n].c_str(), is_selected)) {
                m_selectedIndex = n;
                view->m_onComboChange(m_id, m_selectedIndex);
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
        }

    ImGui::PopID();
};

void Combo::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("placeholder") && widgetPatchDef["placeholder"].is_string()) {
        m_placeholder = widgetPatchDef["placeholder"].template get<std::string>();
    }

    if (widgetPatchDef.contains("options") && widgetPatchDef["options"].is_array()) {
        SetOptions(widgetPatchDef["options"]);
    }
};

bool Combo::HasInternalOps() {
    return true;
}

void Combo::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        auto op = opDef["op"].template get<std::string>();

        if (op == "setSelectedIndex" && opDef.contains("index") && opDef["index"].is_number_integer()) {
            auto index = opDef["index"].template get<int>();
            SetSelectedIndex(index);
        }
    }
};