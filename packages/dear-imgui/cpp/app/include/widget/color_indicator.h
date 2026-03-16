#include "color_helpers.h"
#include "styled_widget.h"

class ColorIndicator final : public StyledWidget {
public:
    ImVec4 m_color;
    bool m_isCircle = false;

    static std::unique_ptr<ColorIndicator> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();

        ImVec4 color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        if (widgetDef.contains("color")) {
            auto maybeColor = extractColor(widgetDef["color"]);
            if (maybeColor.has_value()) {
                color = maybeColor.value();
            }
        }

        std::string shape = "rect";
        if (widgetDef.contains("shape") && widgetDef["shape"].is_string()) {
            shape = widgetDef["shape"].template get<std::string>();
        }

        return std::make_unique<ColorIndicator>(view, id, color, shape, maybeStyle);
    }

    ColorIndicator(XFrames* view, const int id, const ImVec4& color, const std::string& shape, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
        m_type = "color-indicator";
        m_color = color;
        m_isCircle = (shape == "circle");
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;
};
