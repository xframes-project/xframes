#include "widget/text_wrap.h"

TextWrap::TextWrap(ReactImgui* view, const int id, const float& width, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "text-wrap";
    m_handlesChildrenWithinRenderMethod = true;
    m_width = width;
}

void TextWrap::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImGui::PushTextWrapPos(m_width);
    
    Widget::HandleChildren(view, viewport);

    ImGui::PopTextWrapPos();
};

void TextWrap::Patch(const json& widgetPatchDef, ReactImgui* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("width") && widgetPatchDef["width"].is_string()) {
        m_width = widgetPatchDef["width"].template get<float>();
    }
};