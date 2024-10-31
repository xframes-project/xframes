#include "styled_widget.h"

class MultiSlider final : public StyledWidget {
    protected:
        MultiSlider(XFrames* view, const int id, const float min, const float max, const int numValues, const int decimalDigits, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "multi-slider";
            m_numValues = numValues;
            m_values = std::make_unique<float[]>(numValues);
            m_min = min;
            m_max = max;
            m_decimalDigits = decimalDigits;
        }

    public:
        std::unique_ptr<float[]> m_values;
        float m_min;
        float m_max;
        int m_numValues;
        int m_decimalDigits;

        static std::unique_ptr<MultiSlider> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            const auto id = widgetDef["id"].template get<int>();
            const auto numValues = widgetDef.contains("numValues") && widgetDef["numValues"].is_number() ? widgetDef["numValues"].template get<int>() : 2;
            const auto decimalDigits = widgetDef.contains("decimalDigits") && widgetDef["decimalDigits"].is_number() ? widgetDef["decimalDigits"].template get<int>() : 0;

            const auto min = widgetDef.contains("min") && widgetDef["min"].is_number() ? widgetDef["min"].template get<float>() : 0.0f;
            const auto max = widgetDef.contains("max") && widgetDef["max"].is_number() ? widgetDef["max"].template get<float>() : 10.0f;

            if (widgetDef.contains("defaultValues") && widgetDef["defaultValues"].is_array() && widgetDef["defaultValues"].size() == numValues) {
                return makeWidget(view, id, min, max, numValues, decimalDigits, widgetDef["defaultValues"], maybeStyle);
            } else {
                return makeWidget(view, id, min, max, numValues, decimalDigits, maybeStyle);
            }

            // throw std::invalid_argument("Invalid JSON data");
        }

        static std::unique_ptr<MultiSlider> makeWidget(XFrames* view, const int id, const float min, const float max, const int numValues, const int decimalDigits, const json& defaultValues, std::optional<WidgetStyle>& style) {
            MultiSlider instance(view, id, min, max, numValues, decimalDigits, style);

            for (auto& [key, item] : defaultValues.items()) {
                instance.m_values[stoi(key)] = item.template get<float>();
            }

            return std::make_unique<MultiSlider>(std::move(instance));
        }

        static std::unique_ptr<MultiSlider> makeWidget(XFrames* view, const int id, const float min, const float max, const int numValues, const int decimalDigits, std::optional<WidgetStyle>& style) {
            MultiSlider instance(view, id, min, max, numValues, decimalDigits, style);

            return std::make_unique<MultiSlider>(std::move(instance));
        }

        static YGSize Measure(const YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size{};
            const auto context = YGNodeGetContext(node);
            if (context) {
                const auto widget = static_cast<MultiSlider*>(context);

                size.width = static_cast<float>(widget->m_numValues) * 60.0f;
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
