#include <string>

#include "widget/tree_node.h"
#include "widget/styled_widget.h"

TreeNode::TreeNode(XFrames* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
    m_type = "tree-node";
    m_handlesChildrenWithinRenderMethod = true;
}

void TreeNode::Init(const json &elementDef) {
    m_label = elementDef.contains("label") ? elementDef["label"].template get<std::string>() : "";
    m_itemId = elementDef.contains("itemId") ? elementDef["itemId"].template get<std::string>() : "";
    m_selectable = elementDef.contains("selectable") ? elementDef["selectable"].template get<bool>() : false;
    m_leaf = elementDef.contains("leaf") ? elementDef["leaf"].template get<bool>() : false;

    if (elementDef.contains("defaultSelected") && elementDef["defaultSelected"].is_boolean()) {
        m_defaultSelected = elementDef["defaultSelected"].template get<bool>();
    }

    if (elementDef.contains("defaultOpen") && elementDef["defaultOpen"].is_boolean()) {
        m_defaultOpen = elementDef["defaultOpen"].template get<bool>();
    }

    if (elementDef.contains("selected") && elementDef["selected"].is_boolean()) {
        m_selected = elementDef["selected"].template get<bool>();
    }

    if (elementDef.contains("open") && elementDef["open"].is_boolean()) {
        m_open = elementDef["open"].template get<bool>();
    }

    if (!m_open.has_value()) {
        m_localOpen = m_defaultOpen.value_or(false);
    }

    if (!m_selected.has_value()) {
        m_localSelected = m_defaultSelected.value_or(false);
    }
};

void TreeNode::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    YGNodeStyleSetMargin(m_layoutNode->m_node, YGEdgeBottom, m_view->GetWidgetFontSize(this));

    ImGui::PushID(m_id);

    // ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanTextWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_NoAutoOpenOnLog;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_NoAutoOpenOnLog;

    if (m_selected.has_value()) {
        if (m_selected.value()) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
    } else if (m_localSelected.value_or(false)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (m_leaf) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    bool open;

    if (m_open.has_value()) {
        open = m_open.value();
    } else {
        open = m_localOpen.value_or(false);
    }

    ImGui::SetNextItemOpen(open, ImGuiCond_Always);

    ImGui::TreeNodeEx(m_label.c_str(), flags);

    if (ImGui::IsItemClicked()) {
        view->m_onClick(m_id);

        if (!m_open.has_value()) {
            m_localOpen.emplace(!m_localOpen.value_or(false));
        }

        if (m_selectable && !m_selected.has_value()) {
            m_localSelected.emplace(!m_localSelected.value_or(false));
        }
    }

    if (open && m_leaf == false) {
        SetChildrenDisplay(view, YGDisplayFlex);

        const float left = YGNodeLayoutGetLeft(m_layoutNode->m_node);
        const float top = YGNodeLayoutGetTop(m_layoutNode->m_node);
        const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
        const float height = YGNodeLayoutGetHeight(m_layoutNode->m_node);

        ImGui::SetCursorPos(ImVec2(left, top + m_view->GetWidgetFontSize(this))); // account for header

        ImGui::BeginChild("##", ImVec2(width, height), ImGuiChildFlags_None);

        ImGui::SetCursorPos(ImVec2(0, 0));

        Widget::HandleChildren(view, viewport);

        ImGui::EndChild();
    } else {
        SetChildrenDisplay(view, YGDisplayNone);
    }

    ImGui::PopID();
};

void TreeNode::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("label") && widgetPatchDef["label"].is_string()) {
        m_label = widgetPatchDef["label"].template get<std::string>();
    }

    if (widgetPatchDef.contains("selected") && widgetPatchDef["selected"].is_boolean()) {
        SetSelected(widgetPatchDef["selected"].template get<bool>());
    }

    if (widgetPatchDef.contains("selectable") && widgetPatchDef["selectable"].is_boolean()) {
        SetSelectable(widgetPatchDef["selectable"].template get<bool>());
    }

    if (widgetPatchDef.contains("open") && widgetPatchDef["open"].is_boolean()) {
        SetOpen(widgetPatchDef["open"].template get<bool>());
    }
};

void TreeNode::SetSelectable(bool selectable) {
    m_selectable = selectable;

    if (m_selectable == false) {
        m_selected = false;
        m_localSelected = false;
    }
};

void TreeNode::SetSelected(bool selected) {
    if (selected == false || m_selectable) {
        m_selected = selected;
    }
};

void TreeNode::SetOpen(bool open) {
    m_open = open;
};

void TreeNode::SetLeaf(bool leaf) {
    m_leaf = leaf;
};

bool TreeNode::HasCustomWidth() {
    return false;
}

bool TreeNode::HasCustomHeight() {
    return false;
}

bool TreeNode::HasInternalOps() {
    return true;
}

void TreeNode::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        auto op = opDef["op"].template get<std::string>();

        if (op == "setSelected" && opDef.contains("selected") && opDef["selected"].is_boolean()) {
            SetSelected(opDef["selected"].template get<bool>());
        } else if (op == "setSelectable" && opDef.contains("selectable") && opDef["selectable"].is_boolean()) {
            SetSelectable(opDef["selectable"].template get<bool>());
        } else if (op == "setOpen" && opDef.contains("open") && opDef["open"].is_boolean()) {
            SetOpen(opDef["open"].template get<bool>());
        } else if (op == "setLeaf" && opDef.contains("leaf") && opDef["leaf"].is_boolean()) {
            SetLeaf(opDef["leaf"].template get<bool>());
        }
    }
};