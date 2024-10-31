#include "styled_widget.h"

class InputText final : public StyledWidget {
    protected:
        inline static ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_NoUndoRedo;

        static int InputTextCb(ImGuiInputTextCallbackData* data);

        InputText(XFrames* view, const int id, const std::string& defaultValue, const std::string& hint, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "input-text";
            m_bufferPointer = std::make_unique<char[]>(100);
            m_defaultValue = defaultValue;
            m_hint = hint;

            if (!defaultValue.empty()) {
                strncpy(m_bufferPointer.get(), defaultValue.c_str(), 99);
            }
        }

    public:
        std::unique_ptr<char[]> m_bufferPointer;
        std::string m_defaultValue;
        std::string m_hint;

        static std::unique_ptr<InputText> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            const auto id = widgetDef["id"].template get<int>();
            const auto defaultValue = widgetDef.contains("defaultValue") && widgetDef["defaultValue"].is_string() ? widgetDef["defaultValue"].template get<std::string>() : "";
            const auto hint = widgetDef.contains("hint") && widgetDef["hint"].is_string() ? widgetDef["hint"].template get<std::string>() : "";

            return makeWidget(view, id, defaultValue, hint, maybeStyle);

            // throw std::invalid_argument("Invalid JSON data");
        }

        static std::unique_ptr<InputText> makeWidget(XFrames* view, const int id, const std::string& defaultValue, const std::string& hint, std::optional<WidgetStyle>& style) {
            InputText instance(view, id, defaultValue, hint, style);
            return std::make_unique<InputText>(std::move(instance));
        }

        static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size{};
            const auto context = YGNodeGetContext(node);

            if (context) {
                const auto widget = static_cast<InputText*>(context);

                // TODO: we may want to define default widths similarly to how browsers do
                size.width = widget->m_view->GetWidgetFontSize(widget) * 10;
                // TODO: we likely need to compute this based on the associated font, based on the widget's style
                size.height = widget->m_view->GetFrameHeight(widget);
            }

            return size;
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        bool HasInternalOps() override;

        void HandleInternalOp(const json& opDef) override;

        void SetValue(std::string& value) {
            strncpy(m_bufferPointer.get(), value.c_str(), 99);
        }

        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }
};
