#include <imgui.h>

#include "widget/separator_text.h"
#include "xframes.h"

void SeparatorText::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("label") && widgetPatchDef["label"].is_string()) {
        m_label = widgetPatchDef["label"].template get<std::string>();
    }
};

void SeparatorText::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::SeparatorText(m_label.c_str());
};