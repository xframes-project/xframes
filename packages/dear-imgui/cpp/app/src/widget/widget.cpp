#include <GLES3/gl3.h>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>

#include "shared.h"
#include "element/element.h"
#include "widget/widget.h"
#include "reactimgui.h"

using json = nlohmann::json;

Widget::Widget(ReactImgui* view, const int id) : Element(view, id, false, false, false) {
    m_handlesChildrenWithinRenderMethod = false;
}

const char* Widget::GetElementType() {
    return "widget";
};

// todo: seems redundant
void Widget::HandleChildren(ReactImgui* view, const std::optional<ImRect>& viewport) {
    view->RenderChildren(m_id, std::nullopt);
};

void Widget::SetChildrenDisplay(ReactImgui* view, YGDisplay display) const {
    view->SetChildrenDisplay(m_id, display);
};

void Widget::Patch(const json& elementPatchDef, ReactImgui* view) {
    Element::Patch(elementPatchDef, view);
};

void Widget::PreRender(ReactImgui* view) {};

void Widget::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {};

void Widget::PostRender(ReactImgui* view) {};
