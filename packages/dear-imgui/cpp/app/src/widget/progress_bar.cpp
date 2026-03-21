#include <imgui.h>

#include "widget/progress_bar.h"
#include "xframes.h"

void ProgressBar::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("fraction") && widgetPatchDef["fraction"].is_number()) {
        m_fraction = widgetPatchDef["fraction"].template get<float>();
    }

    if (widgetPatchDef.contains("overlay") && widgetPatchDef["overlay"].is_string()) {
        m_overlay = widgetPatchDef["overlay"].template get<std::string>();
    }
};

void ProgressBar::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    const char* overlay_ptr = m_overlay.empty() ? nullptr : m_overlay.c_str();
    const float w = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    ImGui::ProgressBar(m_fraction, ImVec2(w, 0), overlay_ptr);
};
