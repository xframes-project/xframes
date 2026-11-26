#include "styled_widget.h"

class SeparatorText final : public StyledWidget {
public:
    std::string m_label;

    static std::unique_ptr<SeparatorText> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        if (!widgetDef.contains("label") || !widgetDef["label"].is_string()) {
            throw std::invalid_argument("Missing label or not a string");
        }

        auto id = widgetDef["id"].template get<int>();
        auto label = widgetDef["label"].template get<std::string>();

        return std::make_unique<SeparatorText>(view, id, label, maybeStyle);
    }

    // ImGui::SeparatorText reserves its label and the separator text padding around it, at least
    // the separator's thickness high, and draws its line on to the edge of the window. It takes the
    // width its parent gives it, such as a column stretching it across, and otherwise that minimum.
    static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
        YGSize size{};
        const auto context = YGNodeGetContext(node);
        if (context) {
            const auto widget = static_cast<SeparatorText*>(context);
            const ImGuiStyle& appStyle = widget->m_view->m_appStyle;
            ImVec2 padding = appStyle.SeparatorTextPadding;
            float thickness = appStyle.SeparatorTextBorderSize;
            if (widget->HasCustomStyles()) {
                if (widget->HasCustomStyleVar(ImGuiStyleVar_SeparatorTextPadding)) {
                    const auto styleVar = widget->GetCustomStyleVar(ImGuiStyleVar_SeparatorTextPadding);
                    if (std::holds_alternative<ImVec2>(styleVar)) padding = std::get<ImVec2>(styleVar);
                }
                if (widget->HasCustomStyleVar(ImGuiStyleVar_SeparatorTextBorderSize)) {
                    const auto styleVar = widget->GetCustomStyleVar(ImGuiStyleVar_SeparatorTextBorderSize);
                    if (std::holds_alternative<float>(styleVar)) thickness = std::get<float>(styleVar);
                }
            }
            // ImGui::SeparatorText shows, and sizes, the label only up to a "##".
            const auto label = widget->m_view->CalcTextSize(widget, widget->m_label.c_str(), nullptr, true);
            const float minWidth = label.x + padding.x * 2.0f;

            size.width = widthMode == YGMeasureModeExactly ? width : minWidth;
            size.height = std::max(label.y + padding.y * 2.0f, thickness);
        }

        return size;
    }

    SeparatorText(XFrames* view, const int id, const std::string& label, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
        m_type = "separator-text";
        m_label = label;
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    void Init(const json& elementDef) override {
        Element::Init(elementDef);

        YGNodeSetContext(m_layoutNode->m_node, this);
        YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
    }
};
