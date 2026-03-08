#include <nlohmann/json.hpp>

#include "styled_widget.h"
#include "color_helpers.h"

class ColorPicker final : public StyledWidget {
    protected:
        ColorPicker(XFrames* view, const int id, const ImVec4& color, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "color-picker";
            m_color = color;
        }

    public:
        ImVec4 m_color;

        static std::unique_ptr<ColorPicker> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            const auto id = widgetDef["id"].template get<int>();

            ImVec4 color(1.0f, 1.0f, 1.0f, 1.0f);
            if (widgetDef.contains("defaultColor")) {
                auto maybeColor = extractColor(widgetDef["defaultColor"]);
                if (maybeColor.has_value()) {
                    color = maybeColor.value();
                }
            }

            return makeWidget(view, id, color, maybeStyle);
        }

        static std::unique_ptr<ColorPicker> makeWidget(XFrames* view, const int id, const ImVec4& color, std::optional<WidgetStyle>& style) {
            ColorPicker instance(view, id, color, style);
            return std::make_unique<ColorPicker>(std::move(instance));
        }

        static YGSize Measure(const YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size{};
            const auto context = YGNodeGetContext(node);
            if (context) {
                auto widget = static_cast<ColorPicker*>(context);
                const auto frameHeight = widget->m_view->GetFrameHeight(widget);

                size.width = frameHeight;
                size.height = frameHeight;
            }

            return size;
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        bool HasCustomWidth() override;

        bool HasCustomHeight() override;

        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }
};
