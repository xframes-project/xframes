#include "styled_widget.h"
#include "imgui_stdlib.h"

class InputText final : public StyledWidget {
    protected:
        static int InputTextCb(ImGuiInputTextCallbackData* data);

        void ComputeFlags() {
            m_inputTextFlags = ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_NoUndoRedo;
            if (m_password)    m_inputTextFlags |= ImGuiInputTextFlags_Password;
            if (m_readOnly)    m_inputTextFlags |= ImGuiInputTextFlags_ReadOnly;
            if (m_numericOnly) m_inputTextFlags |= ImGuiInputTextFlags_CharsDecimal;
        }

        InputText(XFrames* view, const int id, const std::string& defaultValue, const std::string& hint, bool multiline, bool password, bool readOnly, bool numericOnly, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "input-text";
            m_value = defaultValue;
            m_hint = hint;
            m_multiline = multiline;
            m_password = password;
            m_readOnly = readOnly;
            m_numericOnly = numericOnly;
            ComputeFlags();
        }

    public:
        std::string m_value;
        std::string m_hint;
        bool m_multiline = false;
        bool m_password = false;
        bool m_readOnly = false;
        bool m_numericOnly = false;
        ImGuiInputTextFlags m_inputTextFlags = 0;

        static std::unique_ptr<InputText> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            const auto id = widgetDef["id"].template get<int>();
            const auto defaultValue = widgetDef.contains("defaultValue") && widgetDef["defaultValue"].is_string() ? widgetDef["defaultValue"].template get<std::string>() : "";
            const auto hint = widgetDef.contains("hint") && widgetDef["hint"].is_string() ? widgetDef["hint"].template get<std::string>() : "";
            const bool multiline = widgetDef.contains("multiline") && widgetDef["multiline"].is_boolean() && widgetDef["multiline"].template get<bool>();
            const bool password = widgetDef.contains("password") && widgetDef["password"].is_boolean() && widgetDef["password"].template get<bool>();
            const bool readOnly = widgetDef.contains("readOnly") && widgetDef["readOnly"].is_boolean() && widgetDef["readOnly"].template get<bool>();
            const bool numericOnly = widgetDef.contains("numericOnly") && widgetDef["numericOnly"].is_boolean() && widgetDef["numericOnly"].template get<bool>();

            return makeWidget(view, id, defaultValue, hint, multiline, password, readOnly, numericOnly, maybeStyle);
        }

        static std::unique_ptr<InputText> makeWidget(XFrames* view, const int id, const std::string& defaultValue, const std::string& hint, bool multiline, bool password, bool readOnly, bool numericOnly, std::optional<WidgetStyle>& style) {
            InputText instance(view, id, defaultValue, hint, multiline, password, readOnly, numericOnly, style);
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
                if (widget->m_multiline) {
                    size.height = widget->m_view->GetWidgetFontSize(widget) * 5;
                } else {
                    size.height = widget->m_view->GetFrameHeight(widget);
                }
            }

            return size;
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        bool HasInternalOps() override;

        void HandleInternalOp(const json& opDef) override;

        void SetValue(const std::string& value) {
            m_value = value;
        }

        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }
};
