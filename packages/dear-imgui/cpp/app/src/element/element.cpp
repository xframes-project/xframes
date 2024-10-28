#include <cstring>
#include <nlohmann/json.hpp>
#include <imgui_internal.h>

#include "element/element.h"
#include "color_helpers.h"
#include <GLES3/gl3.h>
#include "reactimgui.h"

using json = nlohmann::json;

Element::Element(ReactImgui* view, const int id, const bool isRoot, const bool cull, const bool trackMouseClickEvents) {
    m_type = "node";
    m_id = id;
    m_view = view;
    m_handlesChildrenWithinRenderMethod = true;
    m_isRoot = isRoot;
    m_cull = cull;
    m_trackMouseClickEvents = trackMouseClickEvents;
    m_layoutNode = std::make_unique<LayoutNode>();
}

void Element::Init(const json& elementDef) {
    YGNodeSetContext(m_layoutNode->m_node, this);

    m_elementStyle = ExtractStyle(elementDef);

    ApplyStyle();
};

std::optional<ElementStyle> Element::ExtractStyle(const json& elementDef) {
    std::optional<ElementStyle> maybeStyle;

    ElementStyle elementStyle;

    if (elementDef.contains("style") && elementDef["style"].is_object()) {
        elementStyle.maybeBase = extractStyleParts(elementDef["style"]);
    }

    if (elementDef.contains("hoverStyle") && elementDef["hoverStyle"].is_object()) {
        elementStyle.maybeHover = extractStyleParts(elementDef["hoverStyle"]);
    }

    if (elementDef.contains("activeStyle") && elementDef["activeStyle"].is_object()) {
        elementStyle.maybeActive = extractStyleParts(elementDef["activeStyle"]);
    }

    if (elementDef.contains("disabledStyle") && elementDef["disabledStyle"].is_object()) {
        elementStyle.maybeDisabled = extractStyleParts(elementDef["disabledStyle"]);
    }

    maybeStyle.emplace(elementStyle);

    return maybeStyle;
};

const char* Element::GetType() const {
    return m_type.c_str();
};

std::unique_ptr<Element> Element::makeElement(const json& nodeDef, ReactImgui* view) {
    auto id = nodeDef["id"].template get<int>();
    bool isRoot = (nodeDef.contains("root") && nodeDef["root"].is_boolean()) ? nodeDef["root"].template get<bool>() : false;
    bool cull = (nodeDef.contains("cull") && nodeDef["cull"].is_boolean()) ? nodeDef["cull"].template get<bool>() : false;
    bool trackMouseClickEvents = (nodeDef.contains("trackMouseClickEvents") && nodeDef["trackMouseClickEvents"].is_boolean()) ? nodeDef["trackMouseClickEvents"].template get<bool>() : false;
    auto element = std::make_unique<Element>(view, id, isRoot, cull, trackMouseClickEvents);

    return element;
};

void Element::ResetStyle() {
    m_layoutNode->ResetStyle();
    m_elementStyle.reset();
};

BorderStyle extractBorderStyle(const json& borderStyleDef) {
    BorderStyle borderStyle;

    if (auto maybeColor = extractColor(borderStyleDef["color"]); maybeColor.has_value()) {
        borderStyle.color = maybeColor.value();
    }

    if (borderStyleDef.contains("thickness")) {
        borderStyle.thickness = borderStyleDef["thickness"].template get<float>();
    }

    return borderStyle;
}

ElementStyleParts extractStyleParts(const json& styleDef) {
    ElementStyleParts elementStyleParts;

    // todo: not sure this fake deep copy is necessary, but let's stay safe for now
    // elementStyleParts.styleDef = json::parse(styleDef.dump());
    elementStyleParts.styleDef = styleDef;

    if (styleDef.contains("backgroundColor")) {
        if (auto maybeColor = extractColor(styleDef["backgroundColor"]); maybeColor.has_value()) {
            elementStyleParts.backgroundColor = maybeColor.value();
        }
    }

    if (styleDef.contains("border")) {
        elementStyleParts.borderAll = extractBorderStyle(styleDef["border"]);
    }

    if (styleDef.contains("borderTop")) {
        elementStyleParts.borderTop = extractBorderStyle(styleDef["borderTop"]);
    }

    if (styleDef.contains("borderRight")) {
        elementStyleParts.borderRight = extractBorderStyle(styleDef["borderRight"]);
    }

    if (styleDef.contains("borderBottom")) {
        elementStyleParts.borderBottom = extractBorderStyle(styleDef["borderBottom"]);
    }

    if (styleDef.contains("borderLeft")) {
        elementStyleParts.borderLeft = extractBorderStyle(styleDef["borderLeft"]);
    }

    if (styleDef.contains("rounding")) {
        elementStyleParts.rounding = styleDef["rounding"].template get<float>();
    }

    if (styleDef.contains("roundCorners") && styleDef["roundCorners"].is_array()) {
        const auto roundCorners = styleDef["roundCorners"].template get<std::vector<std::string>>();

        elementStyleParts.drawFlags = std::accumulate(
            roundCorners.begin(),
            roundCorners.end(),
            static_cast<ImDrawFlags>(ImDrawFlags_None),
            cornersToDrawFlags
        );
    }

    return elementStyleParts;
}

void Element::ApplyStyle() {
    if (m_elementStyle.has_value()) {
        auto state = GetState();

        switch (state) {
            case ElementState_Hover: {
                if (m_elementStyle.value().maybeHover.has_value()) {
                    m_layoutNode->ApplyStyle(m_elementStyle.value().maybeHover.value().styleDef);
                }
                break;
            }
            case ElementState_Active: {
                if (m_elementStyle.value().maybeActive.has_value()) {
                    m_layoutNode->ApplyStyle(m_elementStyle.value().maybeActive.value().styleDef);
                }
                break;
            }
            case ElementState_Disabled: {
                if (m_elementStyle.value().maybeDisabled.has_value()) {
                    m_layoutNode->ApplyStyle(m_elementStyle.value().maybeDisabled.value().styleDef);
                }
                break;
            }

            default: {
                if (m_elementStyle.value().maybeBase.has_value()) {
                    m_layoutNode->ApplyStyle(m_elementStyle.value().maybeBase.value().styleDef);
                }
                break;
            }
        }
    }
}

const char* Element::GetElementType() {
    // todo: should return "element"?
    return "node";
};

ImRect Element::GetScrollingAwareViewport() {
    auto innerClipRect = ImGui::GetCurrentWindow()->InnerClipRect;
    auto clippedSize = ImVec2(innerClipRect.Max.x - innerClipRect.Min.x, innerClipRect.Max.y - innerClipRect.Min.y);
    auto viewport = ImRect(ImGui::GetCurrentWindow()->Scroll.x, ImGui::GetCurrentWindow()->Scroll.y, ImGui::GetCurrentWindow()->Scroll.x + clippedSize.x, ImGui::GetCurrentWindow()->Scroll.y + clippedSize.y);

    // printf("%d %d %d %d\n", (int)viewport.Min.x, (int)viewport.Min.y, (int)viewport.Max.x, (int)viewport.Max.y);

    return viewport;
};

void Element::HandleChildren(ReactImgui* view, const std::optional<ImRect>& parentViewport) {
    if (parentViewport.has_value()) {
        // printf("%d has parent viewport\n", m_id);

        view->RenderChildren(m_id, parentViewport);
    } else if (m_cull) {
        auto viewport = GetScrollingAwareViewport();

        view->RenderChildren(m_id, viewport);
    } else {
        view->RenderChildren(m_id);
    }
};

float Element::GetLayoutLeftFromParentNode(YGNodeRef node, float left) {
    auto owner = YGNodeGetOwner(node);

    float currentNodeTop = YGNodeLayoutGetLeft(node);

    if (!YGFloatIsUndefined(currentNodeTop)) {
        left += currentNodeTop;
    }

    if (owner == nullptr) {
        return left;
    } else {
        return GetLayoutTopFromParentNode(owner, left);
    }
}

float Element::GetLayoutTopFromParentNode(YGNodeRef node, float top) {
    auto owner = YGNodeGetOwner(node);

    float currentNodeTop = YGNodeLayoutGetTop(node);

    if (!YGFloatIsUndefined(currentNodeTop)) {
        top += currentNodeTop;
    }

    if (owner == nullptr) {
        return top;
    } else {
        return GetLayoutTopFromParentNode(owner, top);
    }
}

void Element::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();

    if (!m_layoutNode->m_node) {
        printf("m_node not set\n");
    }

    YGNodeRef owner = YGNodeGetOwner(m_layoutNode->m_node);
    if (owner == nullptr) { // root
        YGNodeCalculateLayout(m_layoutNode->m_node, contentRegionAvail.x, contentRegionAvail.y, YGDirectionLTR);
    }

    const float left = YGNodeLayoutGetLeft(m_layoutNode->m_node);
    const float top = YGNodeLayoutGetTop(m_layoutNode->m_node);
    const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    const float height = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    ImGui::SetCursorPos(ImVec2(left, top));
    ImGui::PushID(m_id);

    const auto size = ImVec2(width, height);

    ImGui::BeginChild("##", size, ImGuiChildFlags_None);

    if (HasStyle(GetState())) {
        DrawBaseEffects();
    }

    // todo: this breaks the chain of events
    if (m_trackMouseClickEvents && size.x != 0.0f && size.y != 0.0f) {
        if (ImGui::InvisibleButton("###", size)) {
            view->m_onClick(m_id);
        }

        const auto isActive = ImGui::IsItemActive();
        auto activeStateChanged = false;

        if (m_isActive != isActive) {
            activeStateChanged = true;
            m_isActive = isActive;
        }

        if (activeStateChanged) {
            ApplyStyle();
        }
    }

    HandleChildren(view, viewport);

    ImGui::EndChild();
    ImGui::PopID();
};

bool Element::HasStyle(ElementState state) {
    if (!m_elementStyle.has_value()) {
        return false;
    }

    switch (state) {
        case ElementState_Hover: {
            if (m_elementStyle.value().maybeHover.has_value()) {
                return true;
            }
        }
        case ElementState_Active: {
            if (m_elementStyle.value().maybeActive.has_value()) {
                return true;
            }
        }
        case ElementState_Disabled: {
            if (m_elementStyle.value().maybeDisabled.has_value()) {
                return true;
            }
        }

        default: break;
    }

    return m_elementStyle.value().maybeBase.has_value();
}

// todo: copy approach to styled_widgets.cpp ?
const std::optional<ElementStyleParts>& Element::GetElementStyleParts(ElementState state) const {
    if (!m_elementStyle.has_value()) {
        return std::nullopt;
    }

    switch (state) {
        case ElementState_Hover: {
            if (m_elementStyle.value().maybeHover.has_value()) {
                return m_elementStyle.value().maybeHover;
            }
        }
        case ElementState_Active: {
            if (m_elementStyle.value().maybeActive.has_value()) {
                return m_elementStyle.value().maybeActive;
            }
        }
        case ElementState_Disabled: {
            if (m_elementStyle.value().maybeDisabled.has_value()) {
                return m_elementStyle.value().maybeDisabled;
            }
        }

        default: break;
    }

    return m_elementStyle.value().maybeBase;
}

void Element::DrawBaseEffects() const {
    const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    const float height = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    const auto size = ImVec2(width, height);

    if (size.x != 0 && size.y != 0) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1 = ImVec2(p0.x + width, p0.y + height);

        const auto& maybeStyle = GetElementStyleParts(GetState());

        if (maybeStyle.has_value()) {
            if (maybeStyle.value().backgroundColor.has_value()) {
                const ImU32 col = ImColor(maybeStyle.value().backgroundColor.value());

                drawList->AddRectFilled(
                    p0,
                    p1,
                    col,
                    maybeStyle.value().rounding.value_or(0),
                    maybeStyle.value().drawFlags
                );
            }

            if (maybeStyle.value().borderAll.has_value()) {
                const ImU32 col = ImColor(maybeStyle.value().borderAll.value().color);

                drawList->AddRect(
                    p0,
                    p1,
                    col,
                    maybeStyle.value().rounding.value_or(0),
                    maybeStyle.value().drawFlags,
                    maybeStyle.value().borderAll.value().thickness
                );

                // auto rounding = m_baseDrawStyle.value().rounding.value_or(0);
                // auto thickness = m_baseDrawStyle.value().borderAll.value().thickness;

                // todo: have to figure out how to implement rounded corners.....
                // drawList->PathArcToFast(ImVec2(p0.x + rounding, p1.y - rounding), rounding, 3, 6); // bottom-left
                // drawList->PathArcToFast(ImVec2(p0.x + rounding, p0.y + rounding), rounding, 6, 9); // top-left
                // drawList->PathArcToFast(ImVec2(p1.x - rounding, p0.y + rounding), rounding, 9, 12); // top-right
                // drawList->PathArcToFast(ImVec2(p1.x - rounding, p1.y - rounding), rounding, 0, 3); // bottom-right

                // drawList->PathStroke(col, ImDrawFlags_Closed, m_baseDrawStyle.value().borderThickness.value_or(1.0f));
            }

            if (maybeStyle.value().borderTop.has_value()) {
                drawList->AddLine(
                    ImVec2(p0.x, p0.y),
                    ImVec2(p1.x, p0.y),
                    ImColor(maybeStyle.value().borderTop.value().color),
                    maybeStyle.value().borderTop.value().thickness
                );
            }
            if (maybeStyle.value().borderRight.has_value()) {
                auto thickness = maybeStyle.value().borderRight.value().thickness;

                drawList->AddLine(
                    ImVec2(p1.x - thickness, p0.y),
                    ImVec2(p1.x - thickness, p1.y),
                    ImColor(maybeStyle.value().borderRight.value().color),
                    maybeStyle.value().borderRight.value().thickness
                );
            }
            if (maybeStyle.value().borderBottom.has_value()) {
                auto thickness = maybeStyle.value().borderBottom.value().thickness;

                drawList->AddLine(
                    ImVec2(p0.x, p1.y - thickness),
                    ImVec2(p1.x, p1.y - thickness),
                    ImColor(maybeStyle.value().borderBottom.value().color),
                    maybeStyle.value().borderBottom.value().thickness
                );
            }
            if (maybeStyle.value().borderLeft.has_value()) {
                drawList->AddLine(
                    ImVec2(p0.x, p1.y),
                    ImVec2(p0.x, p0.y),
                    ImColor(maybeStyle.value().borderLeft.value().color),
                    maybeStyle.value().borderLeft.value().thickness
                );
            }
        }
    }
};

bool Element::ShouldRenderContent(const std::optional<ImRect>& viewport) const {
    if (m_type != "node") {
        // TODO: are we sure about this? Limits culling of content to nodes - not their contents. Hence, quite coarse at the moment.
        return true;
    }

    const float left = YGNodeLayoutGetLeft(m_layoutNode->m_node);
    const float top = YGNodeLayoutGetTop(m_layoutNode->m_node);
    const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    const float height = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    if (viewport.has_value()) {
        // printf("%d %d %d %d\n", (int)viewport.value().Min.x, (int)viewport.value().Max.x, (int)viewport.value().Min.y, (int)viewport.value().Max.y);
        // if (left < viewport.value().Min.x || left > viewport.value().Max.x || top < viewport.value().Min.y || top > viewport.value().Max.y) {
        if ((left + width) < viewport.value().Min.x || (top + height) < viewport.value().Min.y) {
            // printf("Not rendering content (before) for %d\n", m_id);
            // Completely outside (before) the viewport
            return false;
        }

        if (left > viewport.value().Max.x || top > viewport.value().Max.y) {
            // printf("Not rendering content (after) for %d\n", m_id);
            // Completely outside (after) the viewport
            return false;
        }
    }

    return true;
}

bool Element::ShouldRender(ReactImgui* view) const {
    return LayoutNode::ShouldRender(m_layoutNode->m_node);
};

void Element::PreRender(ReactImgui* view) {};

void Element::PostRender(ReactImgui* view) {
    const auto isHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone);

    auto hoveredStateChanged = false;

    if (m_isHovered != isHovered) {
        hoveredStateChanged = true;
        m_isHovered = isHovered;
    }

    if (hoveredStateChanged) {
        ApplyStyle();
    }
};

void Element::Patch(const json& elementPatchDef, ReactImgui* view) {
    m_elementStyle = ExtractStyle(elementPatchDef);

    ApplyStyle();
};

bool Element::HasInternalOps() {
    return false;
};

void Element::HandleInternalOp(const json& opDef) {};

// todo: what about the other states?
// todo: also, this is currently called multiple times - unnecessarily?
ElementState Element::GetState() const {
    if (m_isActive) {
        return ElementState_Active;
    }
    if (m_isHovered) {
        return ElementState_Hover;
    }

    return ElementState_Base;
};
