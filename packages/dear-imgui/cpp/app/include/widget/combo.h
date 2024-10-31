#include "styled_widget.h"

class Combo final : public StyledWidget {
    protected:
        Combo(XFrames* view, const int id, const std::string& placeholder, const int initialSelectedIndex, const std::vector<std::string>& options, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "combo";
            m_placeholder = placeholder;
            m_options = options;

            if (initialSelectedIndex > -1) {
                m_selectedIndex = initialSelectedIndex;
            }
        }

    public:
        int m_selectedIndex = -1;
        std::string m_placeholder;
        std::vector<std::string> m_options;

        static std::unique_ptr<Combo> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            const auto id = widgetDef["id"].template get<int>();
            const auto initialSelectedIndex = widgetDef.contains("initialSelectedIndex") && widgetDef["initialSelectedIndex"].is_number()
                ? widgetDef["initialSelectedIndex"].template get<int>()
                : -1;
            const auto placeholder = widgetDef.contains("placeholder") ? widgetDef["placeholder"].template get<std::string>() : "";

            std::vector<std::string> options;

            if (widgetDef.contains("options") && widgetDef["options"].is_array()) {
                for (const auto &[key, item] : widgetDef["options"].items()) {
                    if (item.is_string()) {
                        options.emplace_back(item.template get<std::string>());
                    }
                }
            }

            return makeWidget(view, id, placeholder, initialSelectedIndex, options, maybeStyle);

            // throw std::invalid_argument("Invalid JSON data");
        }

        static std::unique_ptr<Combo> makeWidget(XFrames* view, const int id, const std::string& placeholder, const int initialSelectedIndex, const std::vector<std::string>& options, std::optional<WidgetStyle>& style) {
            Combo instance(view, id, placeholder, initialSelectedIndex, options, style);
            return std::make_unique<Combo>(std::move(instance));
        }

        static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size{};
            const auto context = YGNodeGetContext(node);

            if (context) {
                const auto widget = static_cast<Combo*>(context);
                // TODO: we may want to define default widths similarly to how browsers do
                size.width = widget->m_view->GetWidgetFontSize(widget) * 10;
                // TODO: we likely need to compute this based on the associated font, based on the widget's style
                size.height = widget->m_view->GetFrameHeight(widget);
            }

            return size;
        }

        void SetOptions(const json& options) {
            m_options.clear();

            if (options.is_array()) {
                for (const auto &[key, item] : options.items()) {
                    if (item.is_string()) {
                        m_options.emplace_back(item.template get<std::string>());
                    }
                }
            }
        }

        void SetOptions(const std::vector<std::string>& options) {
            m_options = options;
        }

        void SetSelectedIndex(int selectedIndex) {
            m_selectedIndex = selectedIndex;
        }

        void ClearSelectedIndex() {
            m_selectedIndex = -1;
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        bool HasInternalOps() override;

        void HandleInternalOp(const json& opDef) override;

        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }
};
