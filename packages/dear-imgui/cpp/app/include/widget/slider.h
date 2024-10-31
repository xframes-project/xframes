#include "styled_widget.h"

class Slider final : public StyledWidget {
    protected:
        Slider(XFrames* view, const int id, const float defaultValue, const float min, const float max, const std::string& sliderType, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "slider";
            m_sliderType = sliderType;
            m_value = defaultValue;
            m_min = min;
            m_max = max;

            if (m_min > m_max) {
                m_min = m_max;
            }
        }

    public:
        std::string m_sliderType;
        float m_value;
        float m_min;
        float m_max;

        static std::unique_ptr<Slider> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            const auto id = widgetDef["id"].template get<int>();
            const auto defaultValue = widgetDef.contains("defaultValue") && widgetDef["defaultValue"].is_number() ? widgetDef["defaultValue"].template get<float>() : 0.0f;
            const auto min = widgetDef.contains("min") && widgetDef["min"].is_number() ? widgetDef["min"].template get<float>() : 0.0f;
            const auto max = widgetDef.contains("max") && widgetDef["max"].is_number() ? widgetDef["max"].template get<float>() : 10.0f;
            const auto sliderType = widgetDef.contains("sliderType") && widgetDef["sliderType"].is_string() ? widgetDef["sliderType"].template get<std::string>() : "default";

            return makeWidget(view, id, defaultValue, min, max, sliderType, maybeStyle);

            // throw std::invalid_argument("Invalid JSON data");
        }

        static std::unique_ptr<Slider> makeWidget(XFrames* view, const int id, const float defaultValue, const float min, const float max, const std::string& sliderType, std::optional<WidgetStyle>& style) {
            Slider instance(view, id, defaultValue, min, max, sliderType, style);
            return std::make_unique<Slider>(std::move(instance));
        }

        static YGSize Measure(const YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size{};
            const auto context = YGNodeGetContext(node);
            if (context) {
                auto widget = static_cast<Slider*>(context);

                size.width = 60.0f; // TODO: This is rather arbitrary
                size.height = widget->m_view->GetFrameHeight(widget);
            }

            return size;
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }
};
