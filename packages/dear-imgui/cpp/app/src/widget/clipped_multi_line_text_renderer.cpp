#include "widget/styled_widget.h"
#include "widget/clipped_multi_line_text_renderer.h"

void ClippedMultiLineTextRenderer::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    const char* buf = m_textBuffer.begin();
    const char* buf_end = m_textBuffer.end();
    ImGuiListClipper clipper;
    clipper.Begin(m_lineOffsets.Size);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    while (clipper.Step()) {
        for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
            const char* line_start = buf + m_lineOffsets[line_no];
            const char* line_end = (line_no + 1 < m_lineOffsets.Size) ? (buf + m_lineOffsets[line_no + 1] - 1) : buf_end;
            ImGui::TextUnformatted(line_start, line_end);
        }
    }

    ImGui::PopStyleVar();

    // ImGui::Text("%d lines", m_lineOffsets.Size);

    ImGui::PopID();
};

void ClippedMultiLineTextRenderer::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("numberOfLines") && widgetPatchDef["numberOfLines"].is_number_unsigned()) {
        SetNumberOfLines(widgetPatchDef["numberOfLines"].template get<int>());
    }
};