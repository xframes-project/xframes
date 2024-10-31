#include <cstring>
#include <string>
#include <nlohmann/json.hpp>

#include "shared.h"
#include "element/element.h"
#include "widget/widget.h"
#include "xframes.h"

using json = nlohmann::json;

Widget::Widget(XFrames* view, const int id) : Element(view, id, false, false, false) {
    m_handlesChildrenWithinRenderMethod = false;
}

const char* Widget::GetElementType() {
    return "widget";
};

// todo: seems redundant
void Widget::HandleChildren(XFrames* view, const std::optional<ImRect>& viewport) {
    view->RenderChildren(m_id, std::nullopt);
};

void Widget::SetChildrenDisplay(XFrames* view, YGDisplay display) const {
    view->SetChildrenDisplay(m_id, display);
};

void Widget::Patch(const json& elementPatchDef, XFrames* view) {
    Element::Patch(elementPatchDef, view);
};

void Widget::PreRender(XFrames* view) {};

void Widget::Render(XFrames* view, const std::optional<ImRect>& viewport) {};

void Widget::PostRender(XFrames* view) {};
