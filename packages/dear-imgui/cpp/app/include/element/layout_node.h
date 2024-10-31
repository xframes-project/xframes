#include <yoga/Yoga.h>
#include <yoga/YGNodeStyle.h>
#include <nlohmann/json.hpp>
#include <yoga/node/Node.h>
#include <yoga/style/Style.h>

#include "shared.h"

using json = nlohmann::json;

#pragma once

class XFrames;

class LayoutNode {
    std::unordered_map<std::string, std::unique_ptr<IVariadicFn>, StringHash, std::equal_to<>> m_setters;

    // Add a setter for any number of parameters
    template <typename... Args>
    void AddSetter(const std::string& key, std::function<void(Args...)> setter) {
        m_setters[key] = std::make_unique<VariadicFnImpl<Args...>>(std::move(setter));
    }

    // Call a setter with variadic arguments
    template <typename... Args>
    void CallSetter(const std::string& key, Args... args) const {
        auto it = m_setters.find(key);
        if (it != m_setters.end()) {
            void* arguments[] = { &args... };
            it->second->call(arguments);
        } else {
            throw std::invalid_argument("Setter not found for key: " + key);
        }
    }

    public:
        YGNodeRef m_node;

        LayoutNode();

        [[nodiscard]] size_t GetChildCount() const;

        void InsertChild(LayoutNode* child, size_t index) const;

        void ApplyStyle(const json& styleDef) const;

        [[nodiscard]] YGNodeRef GetParentNode() const {
            return YGNodeGetParent(m_node);
        }

        static bool ShouldRender(YGNodeConstRef node) {
            const auto parentNode = YGNodeGetParent((YGNodeRef)node);

            return !parentNode || YGNodeStyleGetDisplay(parentNode) != YGDisplayNone;
        }

        void ResetStyle() const;

        void SetDirection(const YGDirection direction) const {
            YGNodeStyleSetDirection(m_node, direction);
        }

        void SetFlexDirection(const YGFlexDirection direction) const {
            YGNodeStyleSetFlexDirection(m_node, direction);
        }

        void SetJustifyContent(const YGJustify justifyContent) const {
            YGNodeStyleSetJustifyContent(m_node, justifyContent);
        }

        void SetAlignContent(const YGAlign alignContent) const {
            YGNodeStyleSetAlignContent(m_node, alignContent);
        }

        void SetAlignItems(const YGAlign alignItems) const {
            YGNodeStyleSetAlignItems(m_node, alignItems);
        }

        void SetAlignSelf(const YGAlign alignSelf) const {
            YGNodeStyleSetAlignSelf(m_node, alignSelf);
        }

        void SetPositionType(const YGPositionType positionType) const {
            YGNodeStyleSetPositionType(m_node, positionType);
        }

        void SetFlexWrap(const YGWrap flexWrap) const {
            YGNodeStyleSetFlexWrap(m_node, flexWrap);
        }

        void SetOverflow(const YGOverflow overflow) const {
            YGNodeStyleSetOverflow(m_node, overflow);
        }

        void SetDisplay(const YGDisplay display) const {
            YGNodeStyleSetDisplay(m_node, display);
        }

        void SetFlex(float flex) const {
            YGNodeStyleSetFlex(m_node, flex);
        }

        void SetFlexGrow(float flexGrow) const {
            YGNodeStyleSetFlexGrow(m_node, flexGrow);
        }

        void SetFlexShrink(const float flexShrink) const {
            YGNodeStyleSetFlexShrink(m_node, flexShrink);
        }

        void SetFlexBasis(const float flexBasis) const {
            YGNodeStyleSetFlexBasis(m_node, flexBasis);
        }

        void SetFlexBasisPercent(const float flexBasisPercent) const {
            YGNodeStyleSetFlexBasisPercent(m_node, flexBasisPercent);
        }

        void SetFlexBasisAuto() const {
            YGNodeStyleSetFlexBasisAuto(m_node);
        }

        void SetPosition(const YGEdge edge, const float points) const {
            YGNodeStyleSetPosition(m_node, edge, points);
        }

        void SetMargin(const YGEdge edge, const float points) const {
            YGNodeStyleSetMargin(m_node, edge, points);
        }

        void SetMarginAuto(const YGEdge edge) const {
            YGNodeStyleSetMarginAuto(m_node, edge);
        }

        void SetPadding(const YGEdge edge, const float points) const {
            YGNodeStyleSetPadding(m_node, edge, points);
        }

        void SetBorder(const YGEdge edge, const float border) const {
            YGNodeStyleSetBorder(m_node, edge, border);
        }

        void SetGap(const YGGutter gutter, const float gapLength) const {
            YGNodeStyleSetGap(m_node, gutter, gapLength);
        }

        void SetGapPercent(const YGGutter gutter, const float percent) const {
            YGNodeStyleSetGapPercent(m_node, gutter, percent);
        }

        void SetAspectRatio(const float aspectRatio) const {
            YGNodeStyleSetAspectRatio(m_node, aspectRatio);
        }

        void SetWidth(const float width) const {
            YGNodeStyleSetWidth(m_node, width);
        }

        void SetWidthPercent(const float percent) const {
            YGNodeStyleSetWidthPercent(m_node, percent);
        }

        void SetWidthAuto() const {
            YGNodeStyleSetWidthAuto(m_node);
        }

        void SetHeight(const float height) const {
            YGNodeStyleSetHeight(m_node, height);
        }

        void SetHeightPercent(const float percent) const {
            YGNodeStyleSetHeightPercent(m_node, percent);
        }

        void SetHeightAuto() const {
            YGNodeStyleSetHeightAuto(m_node);
        }

        void SetMinWidth(const float minWidth) const {
            YGNodeStyleSetMinWidth(m_node, minWidth);
        }

        void SetMinWidthPercent(const float percent) const {
            YGNodeStyleSetMinWidthPercent(m_node, percent);
        }

        void SetMinHeight(const float minHeight) const {
            YGNodeStyleSetMinHeight(m_node, minHeight);
        }

        void SetMinHeightPercent(const float percent) const {
            YGNodeStyleSetMinHeightPercent(m_node, percent);
        }

        void SetMaxWidth(const float maxWidth) const {
            YGNodeStyleSetMaxWidth(m_node, maxWidth);
        }

        void SetMaxWidthPercent(const float percent) const {
            YGNodeStyleSetMaxWidthPercent(m_node, percent);
        }

        void SetMaxHeight(const float maxHeight) const {
            YGNodeStyleSetMaxHeight(m_node, maxHeight);
        }

        void SetMaxHeightPercent(const float percent) const {
            YGNodeStyleSetMaxHeightPercent(m_node, percent);
        }

        facebook::yoga::Style& GetStyle() const {
            return facebook::yoga::resolveRef(m_node)->style();
        }

        template <typename T, typename PropertyValueResolver>
        void ApplyOptionalStyleProperty(const json& styleDef, const std::string& propertyKey, PropertyValueResolver resolver) const {
            if (styleDef.is_object() && styleDef.contains(propertyKey) && styleDef[propertyKey].is_string()) {
                auto rawValue = styleDef[propertyKey].template get<std::string>();
                std::optional<T> value = resolver(rawValue);
                if (value.has_value()) {
                    CallSetter<T>(propertyKey, value.value());
                }
            }
        }

        template <typename T1, typename T2, typename EdgeResolver>
        void ApplyOptionalMultiEdgeStyleProperty(const json& styleDef, const std::string& propertyKey, EdgeResolver edgeResolver) const {
            if (styleDef.is_object() && styleDef.contains(propertyKey) && styleDef[propertyKey].is_object()) {
                for (auto& [key, item] : styleDef[propertyKey].items()) {
                    if (item.is_number()) {
                        std::optional<T1> edge = edgeResolver(key);
                        if (edge.has_value()) {
                            CallSetter<T1, T2>(propertyKey, edge.value(), item.template get<float>());
                        }
                    }
                }
            }
        }
};

