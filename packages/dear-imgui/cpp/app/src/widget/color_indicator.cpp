#include <imgui.h>

#include "widget/color_indicator.h"
#include "xframes.h"

void ColorIndicator::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("color")) {
        auto maybeColor = extractColor(widgetPatchDef["color"]);
        if (maybeColor.has_value()) {
            m_color = maybeColor.value();
        }
    }

    if (widgetPatchDef.contains("shape") && widgetPatchDef["shape"].is_string()) {
        m_shape = widgetPatchDef["shape"].template get<std::string>();
    }
};

void ColorIndicator::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    float w = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    float h = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 col = ImGui::GetColorU32(m_color);

    if (m_shape == "circle") {
        float radius = std::min(w, h) * 0.5f;
        drawList->AddCircleFilled(
            ImVec2(pos.x + w * 0.5f, pos.y + h * 0.5f),
            radius, col);
    } else {
        drawList->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), col);
    }

    ImGui::Dummy(ImVec2(w, h));
};
