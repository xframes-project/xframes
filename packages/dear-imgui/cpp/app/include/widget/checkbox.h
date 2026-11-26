#include <nlohmann/json.hpp>

#include "styled_widget.h"

class Checkbox final : public StyledWidget {
    protected:
        Checkbox(XFrames* view, const int id, const std::string& label, const bool defaultChecked, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "checkbox";
            m_checked = defaultChecked;
            m_label = label;
        }

    public:
        bool m_checked;
        std::string m_label;

        static std::unique_ptr<Checkbox> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            const auto id = widgetDef["id"].template get<int>();
            // checked is the application's value; defaultChecked only seeds a checkbox without one.
            const auto checkedKey = widgetDef.contains("checked") && widgetDef["checked"].is_boolean() ? "checked" : "defaultChecked";
            const auto checked = widgetDef.contains(checkedKey) && widgetDef[checkedKey].is_boolean() ? widgetDef[checkedKey].template get<bool>() : false;
            const auto label = widgetDef.contains("label") && widgetDef["label"].is_string() ? widgetDef["label"].template get<std::string>() : "";

            return makeWidget(view, id, label, checked, maybeStyle);

            // throw std::invalid_argument("Invalid JSON data");
        }

        static std::unique_ptr<Checkbox> makeWidget(XFrames* view, const int id,  const std::string& label, const bool defaultChecked, std::optional<WidgetStyle>& style) {
            Checkbox instance(view, id, label, defaultChecked, style);
            return std::make_unique<Checkbox>(std::move(instance));
        }

        // ImGui::Checkbox draws its square, a frame high, then its label after the item inner
        // spacing, and is as tall as the label with the frame padding above and below. It shows,
        // and sizes, the label only up to a "##".
        static YGSize Measure(const YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size{};
            const auto context = YGNodeGetContext(node);
            if (context) {
                auto widget = static_cast<Checkbox*>(context);
                const ImGuiStyle& appStyle = widget->m_view->m_appStyle;
                float framePaddingY = appStyle.FramePadding.y;
                float innerSpacingX = appStyle.ItemInnerSpacing.x;
                if (widget->HasCustomStyles()) {
                    if (widget->HasCustomStyleVar(ImGuiStyleVar_FramePadding)) {
                        const auto styleVar = widget->GetCustomStyleVar(ImGuiStyleVar_FramePadding);
                        if (std::holds_alternative<ImVec2>(styleVar)) framePaddingY = std::get<ImVec2>(styleVar).y;
                    }
                    if (widget->HasCustomStyleVar(ImGuiStyleVar_ItemInnerSpacing)) {
                        const auto styleVar = widget->GetCustomStyleVar(ImGuiStyleVar_ItemInnerSpacing);
                        if (std::holds_alternative<ImVec2>(styleVar)) innerSpacingX = std::get<ImVec2>(styleVar).x;
                    }
                }
                const auto square = widget->m_view->GetFrameHeight(widget);
                const auto label = widget->m_view->CalcTextSize(widget, widget->m_label.c_str(), nullptr, true);

                size.width = square + (label.x > 0.0f ? innerSpacingX + label.x : 0.0f);
                size.height = label.y + framePaddingY * 2.0f;
            }

            return size;
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        // Under controlled values a click is the user's edit: ImGui toggles what the checkbox
        // shows, and the committed value returns once the application has seen the click.
        const std::vector<std::string>& ControlledProps() const override {
            static const std::vector<std::string> props = {"checked"};
            return props;
        }

        void ApplyControlled(const json& values) override {
            if (values.contains("checked") && values["checked"].is_boolean()) m_checked = values["checked"].template get<bool>();
        }

        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }
};
