#include <optional>

#include "widget/styled_widget.h"
#include "color_helpers.h"
#include "xframes.h"
#include "imgui_renderer.h"

bool WidgetStyle::HasCustomFont(const std::optional<ElementState> widgetState, XFrames* view) {
    const auto hasBaseValue = maybeBase.has_value() && maybeBase.value().maybeFontIndex.has_value() && view->m_renderer->IsFontIndexValid(maybeBase.value().maybeFontIndex.value());

    switch(widgetState.value_or(ElementState_Base)) {
        case ElementState_Disabled: {
            const auto hasDisabledValue = maybeDisabled.has_value() && maybeDisabled.value().maybeFontIndex.has_value() && view->m_renderer->IsFontIndexValid(maybeDisabled.value().maybeFontIndex.value());
            return hasDisabledValue || hasBaseValue;
        }

        case ElementState_Hover: {
            const auto hasOverValue = maybeHover.has_value() && maybeHover.value().maybeFontIndex.has_value() && view->m_renderer->IsFontIndexValid(maybeHover.value().maybeFontIndex.value());
            return hasOverValue || hasBaseValue;
        }

        case ElementState_Active: {
            const auto hasActiveValue = maybeActive.has_value() && maybeActive.value().maybeFontIndex.has_value() && view->m_renderer->IsFontIndexValid(maybeActive.value().maybeFontIndex.value());
            return hasActiveValue || hasBaseValue;
        }

        default:
            break;
    }

    return hasBaseValue;
}

bool WidgetStyle::HasCustomColors(const std::optional<ElementState> widgetState) const {
    const auto hasBaseValue = maybeBase.has_value() && maybeBase.value().maybeColors.has_value();

    switch(widgetState.value_or(ElementState_Base)) {
        case ElementState_Disabled: {
            const auto hasDisabledValue = maybeDisabled.has_value() && maybeDisabled.value().maybeColors.has_value();
            return hasDisabledValue || hasBaseValue;
        }

        case ElementState_Hover: {
            const auto hasHoverValue = maybeHover.has_value() && maybeHover.value().maybeColors.has_value();
            return hasHoverValue || hasBaseValue;
        }

        case ElementState_Active: {
            const auto hasActiveValue = maybeActive.has_value() && maybeActive.value().maybeColors.has_value();
            return hasActiveValue || hasBaseValue;
        }

        default:
            return hasBaseValue;
    }
}

bool WidgetStyle::HasCustomStyleVars(const std::optional<ElementState> widgetState) const {
    const auto hasBaseValue = maybeBase.has_value() && maybeBase.value().maybeStyleVars.has_value();

    switch(widgetState.value_or(ElementState_Base)) {
        case ElementState_Disabled: {
            const auto hasDisabledValue = maybeDisabled.has_value() && maybeDisabled.value().maybeStyleVars.has_value();
            return hasDisabledValue || hasBaseValue;
        }

        case ElementState_Hover: {
            const auto hasHoverValue = maybeHover.has_value() && maybeHover.value().maybeStyleVars.has_value();
            return hasHoverValue || hasBaseValue;
        }

        case ElementState_Active: {
            const auto hasActiveValue = maybeActive.has_value() && maybeActive.value().maybeStyleVars.has_value();
            return hasActiveValue || hasBaseValue;
        }

        default:
            return hasBaseValue;
    }
}

bool WidgetStyle::HasCustomStyleVar(const std::optional<ElementState> widgetState, const ImGuiStyleVar key) const {
    const auto hasBaseValue = maybeBase.has_value() && maybeBase.value().maybeStyleVars.has_value() && maybeBase.value().maybeStyleVars.value().contains(key);

    switch(widgetState.value_or(ElementState_Base)) {
        case ElementState_Disabled: {
            const auto hasDisabledValue = maybeDisabled.has_value() && maybeDisabled.value().maybeStyleVars.has_value() && maybeDisabled.value().maybeStyleVars.value().contains(key);
            return hasDisabledValue || hasBaseValue;
        }

        case ElementState_Hover: {
            const auto hasHoverValue = maybeHover.has_value() && maybeHover.value().maybeStyleVars.has_value() && maybeHover.value().maybeStyleVars.value().contains(key);
            return hasHoverValue || hasBaseValue;
        }

        case ElementState_Active: {
            const auto hasActiveValue = maybeActive.has_value() && maybeActive.value().maybeStyleVars.has_value() && maybeActive.value().maybeStyleVars.value().contains(key);
            return hasActiveValue || hasBaseValue;
        }

        default:
            return hasBaseValue;
    }
}

StyleColors& WidgetStyle::GetCustomColors(std::optional<ElementState> widgetState) {
    auto& baseColors = maybeBase.value().maybeColors.value();

    switch(widgetState.value_or(ElementState_Base)) {
        case ElementState_Disabled: {
            if (maybeDisabled.has_value() && maybeDisabled.value().maybeColors.has_value()) {
                return maybeDisabled.value().maybeColors.value();
            }
        }

        case ElementState_Hover: {
            if (maybeHover.has_value() && maybeHover.value().maybeColors.has_value()) {
                return maybeHover.value().maybeColors.value();
            }
        }

        case ElementState_Active: {
            if (maybeActive.has_value() && maybeActive.value().maybeColors.has_value()) {
                return maybeActive.value().maybeColors.value();
            }
        }

        default:
            break;
    }

    return baseColors;
}

StyleVars& WidgetStyle::GetCustomStyleVars(std::optional<ElementState> widgetState) {
    auto& baseStyleVars = maybeBase.value().maybeStyleVars.value();

    switch(widgetState.value_or(ElementState_Base)) {
        case ElementState_Disabled: {
            if (maybeDisabled.has_value() && maybeDisabled.value().maybeStyleVars.has_value()) {
                return maybeDisabled.value().maybeStyleVars.value();
            }
        }

        case ElementState_Hover: {
            if (maybeHover.has_value() && maybeHover.value().maybeStyleVars.has_value()) {
                return maybeHover.value().maybeStyleVars.value();
            }
        }

        case ElementState_Active: {
            if (maybeActive.has_value() && maybeActive.value().maybeStyleVars.has_value()) {
                return maybeActive.value().maybeStyleVars.value();
            }
        }

        default:
            break;
    }

    return baseStyleVars;
}

int WidgetStyle::GetCustomFontId(std::optional<ElementState> widgetState, XFrames* view) {
    auto fontIndex = maybeBase.value().maybeFontIndex.value();

    switch(widgetState.value_or(ElementState_Base)) {
        case ElementState_Disabled: {
            if (maybeDisabled.has_value() && maybeDisabled.value().maybeFontIndex.has_value()) {
                return maybeDisabled.value().maybeFontIndex.value();
            }
        }

        case ElementState_Hover: {
            if (maybeHover.has_value() && maybeHover.value().maybeFontIndex.has_value()) {
                return maybeHover.value().maybeFontIndex.value();
            }
        }

        case ElementState_Active: {
            if (maybeActive.has_value() && maybeActive.value().maybeFontIndex.has_value()) {
                return maybeActive.value().maybeFontIndex.value();
            }
        }

        default:
            break;
    }

    return fontIndex;
}

// todo: is it really that difficult to return the variant by reference?
StyleVarValue WidgetStyle::GetCustomStyleVar(std::optional<ElementState> widgetState, ImGuiStyleVar key) {
    // if (HasCustomStyleVar(widgetState, key)) {
    //     return maybeDisabled.value().maybeFontIndex.value();
    // } else
    if (HasCustomStyleVar(ElementState_Base, key)) {
        return maybeBase.value().maybeStyleVars.value().at(key);
    } else {
        StyleVarValue nothing;

        return nothing;
    }
}

WidgetStyleParts extractStyleParts(const json& styleDef, XFrames* view) {
    auto widgetStyleParts = WidgetStyleParts{};

    if (styleDef.contains("font")
        && styleDef["font"].is_object()
        && styleDef["font"]["name"].is_string()
        && styleDef["font"]["size"].is_number_unsigned()) {

        widgetStyleParts.maybeFontIndex.emplace(view->m_renderer->GetFontIndex(
            styleDef["font"]["name"].template get<std::string>(),
            styleDef["font"]["size"].template get<int>()
        ));
    }

    if (styleDef.contains("colors") && styleDef["colors"].is_object()) {
        StyleColors colors;

        for (auto& [key, item] : styleDef["colors"].items()) {
            if (auto maybeColor = extractColor(item); maybeColor.has_value()) {
                colors[stoi(key)] = maybeColor.value();
            }
        }

        if (!colors.empty()) {
            widgetStyleParts.maybeColors.emplace(colors);
        }
    }

    if (styleDef.contains("vars") && styleDef["vars"].is_object()) {
        StyleVars styleVars;

        for (auto& [key, item] : styleDef["vars"].items()) {
            StyleVarValue value;

            if (item.is_array() && item.size() == 2
                 && item[0].is_number_unsigned() && item[1].is_number_unsigned()) { // ImVec2
                value = ImVec2(
                    item[0].template get<float>(),
                    item[1].template get<float>()
                );
            } else if (item.is_number_unsigned()) { // float
                value = item.template get<float>();
            }

            if (value.index() != 0) {
                styleVars[stoi(key)] = value;
            }
        }

        if (!styleVars.empty()) {
            widgetStyleParts.maybeStyleVars.emplace(styleVars);
        }
    }

    return widgetStyleParts;
}

std::optional<WidgetStyle> StyledWidget::ExtractStyle(const json& widgetDef, XFrames* view) {
    std::optional<WidgetStyle> maybeStyle;

    WidgetStyle widgetStyle;

    if (widgetDef.contains("style") && widgetDef["style"].is_object()) {
        widgetStyle.maybeBase = extractStyleParts(widgetDef["style"], view);
    }

    if (widgetDef.contains("hoverStyle") && widgetDef["hoverStyle"].is_object()) {
        widgetStyle.maybeHover = extractStyleParts(widgetDef["hoverStyle"], view);
    }

    if (widgetDef.contains("activeStyle") && widgetDef["activeStyle"].is_object()) {
        widgetStyle.maybeActive = extractStyleParts(widgetDef["activeStyle"], view);
    }

    if (widgetDef.contains("disabledStyle") && widgetDef["disabledStyle"].is_object()) {
        widgetStyle.maybeDisabled = extractStyleParts(widgetDef["disabledStyle"], view);
    }

    maybeStyle.emplace(widgetStyle);

    return maybeStyle;
};

StyledWidget::StyledWidget(XFrames* view, const int id) : Widget(view, id) {}

StyledWidget::StyledWidget(XFrames* view, const int id, std::optional<WidgetStyle>& maybeStyle) : Widget(view, id) {
    if (maybeStyle.has_value()) {
        m_style.emplace(std::make_unique<WidgetStyle>(maybeStyle.value()));
    }
}

void StyledWidget::ReplaceStyle(WidgetStyle& newStyle) {
    m_style.reset();
    m_style.emplace(std::make_unique<WidgetStyle>(newStyle));
};

void StyledWidget::Patch(const json& widgetPatchDef, XFrames* view) {
    Widget::Patch(widgetPatchDef, view);

    // todo: we probably need to test all 4 state objects individually
    auto maybeNewStyle = ExtractStyle(widgetPatchDef, view);

    if (maybeNewStyle.has_value()) {
        ReplaceStyle(maybeNewStyle.value());
    }

    if (YGNodeGetParent(m_layoutNode->m_node) && YGNodeHasMeasureFunc(m_layoutNode->m_node)) {
        YGNodeMarkDirty(m_layoutNode->m_node);
    }
};

bool StyledWidget::HasCustomStyles() const {
    return m_style.has_value();
};

// Assumes m_style is not null, you should call HasCustomStyles() first
bool StyledWidget::HasCustomFont(XFrames* view) const {
    return m_style.value()->HasCustomFont(GetState(), view);
};

// Assumes m_style is not null, you should call HasCustomStyles() first
bool StyledWidget::HasCustomWidth() {
    return !YGFloatIsUndefined(YGNodeLayoutGetWidth(m_layoutNode->m_node));
};

// Assumes m_style is not null, you should call HasCustomStyles() first
bool StyledWidget::HasCustomHeight() {
    return false;
};

// Assumes m_style is not null, you should call HasCustomStyles() first
bool StyledWidget::HasCustomColors() const {
    return m_style.value()->HasCustomColors(GetState());
};

// Assumes m_style is not null, you should call HasCustomStyles() first
bool StyledWidget::HasCustomStyleVars() const {
    return m_style.value()->HasCustomStyleVars(GetState());
};

void StyledWidget::PreRender(XFrames* view) {
    const float left = YGNodeLayoutGetLeft(m_layoutNode->m_node);
    const float top = YGNodeLayoutGetTop(m_layoutNode->m_node);

    if (!YGFloatIsUndefined(left)) {
        // printf("%d %s %f %f\n", m_id, m_type.c_str(), left, top);
        ImGui::SetCursorPos(ImVec2(left, top));
    }

    if (HasCustomWidth()) {
        const float width = YGNodeLayoutGetWidth(m_layoutNode->m_node);
        ImGui::PushItemWidth(width);
    }

    if (HasCustomStyles()) {
        if (HasCustomFont(view)) {
            view->m_renderer->PushFont(m_style.value()->GetCustomFontId(GetState(), view));
        }

        if (HasCustomColors()) {
            for (auto const& [key, val] : m_style.value()->GetCustomColors(GetState())) {
                ImGui::PushStyleColor(key, val);
            }
        }

        if (HasCustomStyleVars()) {
            for (auto const& [key, val] : m_style.value()->GetCustomStyleVars(GetState())) {
                if (std::holds_alternative<float>(val)) {
                    ImGui::PushStyleVar(key, std::get<float>(val));
                } else if (std::holds_alternative<ImVec2>(val)) {
                    ImGui::PushStyleVar(key, std::get<ImVec2>(val));
                } else {
                    // todo: throw?
                }
            }
        }
    }
};

void StyledWidget::PostRender(XFrames* view) {
    if (HasCustomWidth()) {
        ImGui::PopItemWidth();
    }

    if (HasCustomStyles()) {
        if (HasCustomFont(view)) {
            view->m_renderer->PopFont();
        }

        if (HasCustomColors()) {
            ImGui::PopStyleColor(m_style.value()->GetCustomColors(GetState()).size());
        }

        if (HasCustomStyleVars()) {
            // Big, big assumption that this will match exactly the number of style vars being pushed above... Maybe we should actually keep track
            ImGui::PopStyleVar(m_style.value()->GetCustomStyleVars(GetState()).size());
        }
    }

    // todo: need a way to track state changes that does not kill performance
    const auto isHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone);
    const auto isActive = ImGui::IsItemActive();
    const auto isFocused = ImGui::IsItemFocused();

    auto hoveredStateChanged = false;
    auto activeStateChanged = false;
    auto focusedStateChanged = false;

    if (m_isHovered != isHovered) {
        hoveredStateChanged = true;
        m_isHovered = isHovered;
    }

    if (m_isActive != isActive) {
        activeStateChanged = true;
        m_isActive = isActive;
    }

    if (m_isFocused != isFocused) {
        focusedStateChanged = true;
        m_isFocused = isFocused;
    }

    if (hoveredStateChanged || activeStateChanged || focusedStateChanged) {
        ApplyStyle();
    }
};

bool StyledWidget::HasCustomStyleVar(const ImGuiStyleVar key) const {
    if (!m_style.has_value()) {
        return false;
    }

    return m_style.value()->HasCustomStyleVar(GetState(), key);
};

// todo: is it really that difficult to return the variant by reference?
StyleVarValue StyledWidget::GetCustomStyleVar(const ImGuiStyleVar key) const {
    if (!m_style.has_value()) {
        StyleVarValue nothing;

        return nothing;
    }

    return m_style.value()->GetCustomStyleVar(GetState(), key);
};