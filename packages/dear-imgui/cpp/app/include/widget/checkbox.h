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
            const auto defaultChecked = widgetDef.contains("defaultChecked") && widgetDef["defaultChecked"].is_boolean() ? widgetDef["defaultChecked"].template get<bool>() : false;
            const auto label = widgetDef.contains("label") && widgetDef["label"].is_string() ? widgetDef["label"].template get<std::string>() : "";

            return makeWidget(view, id, label, defaultChecked, maybeStyle);

            // throw std::invalid_argument("Invalid JSON data");
        }

        static std::unique_ptr<Checkbox> makeWidget(XFrames* view, const int id,  const std::string& label, const bool defaultChecked, std::optional<WidgetStyle>& style) {
            Checkbox instance(view, id, label, defaultChecked, style);
            return std::make_unique<Checkbox>(std::move(instance));
        }

        static YGSize Measure(const YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size{};
            const auto context = YGNodeGetContext(node);
            if (context) {
                auto widget = static_cast<Checkbox*>(context);
                const auto frameHeight = widget->m_view->GetFrameHeight(widget);

                size.width = frameHeight;
                size.height = frameHeight;
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
