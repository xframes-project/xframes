#include "styled_widget.h"

class Text : public StyledWidget {
    public:
        std::string m_text;

        Text(XFrames* view, int id, const std::string& text, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_text = text;
        }

        static YGSize Measure(const YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size{};
            const auto context = YGNodeGetContext(node);
            if (context) {
                const auto widget = static_cast<Text*>(context);

                size.width = widget->m_view->CalcTextSize(widget, widget->m_text.c_str()).x;
                size.height = widget->m_view->GetWidgetFontSize(widget);
            }

            return size;
        }

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }
};

class BulletText final : public Text {
    public:
        static std::unique_ptr<BulletText> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view);

        BulletText(XFrames* view, const int id, const std::string& text, std::optional<WidgetStyle>& style) : Text(view, id, text, style) {
            m_type = "bullet-text";
        }

        // ImGui::BulletText draws a bullet as wide as the font before the text, and twice the
        // frame padding between them when there is text: the text's measure plus that much.
        static YGSize Measure(const YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size = Text::Measure(node, width, widthMode, height, heightMode);
            const auto context = YGNodeGetContext(node);
            if (context) {
                const auto widget = static_cast<BulletText*>(static_cast<Text*>(context));
                float framePaddingX = widget->m_view->m_appStyle.FramePadding.x;
                if (widget->HasCustomStyles() && widget->HasCustomStyleVar(ImGuiStyleVar_FramePadding)) {
                    const auto styleVar = widget->GetCustomStyleVar(ImGuiStyleVar_FramePadding);
                    if (std::holds_alternative<ImVec2>(styleVar)) framePaddingX = std::get<ImVec2>(styleVar).x;
                }
                const float textWidth = size.width;
                size.width = widget->m_view->GetWidgetFontSize(widget) + (textWidth > 0.0f ? textWidth + framePaddingX * 2.0f : 0.0f);
            }

            return size;
        }

        void Init(const json& elementDef) override {
            Text::Init(elementDef);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
};

class UnformattedText final : public Text {
    public:
        static std::unique_ptr<UnformattedText> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view);

        UnformattedText(XFrames* view, const int id, const std::string& text, std::optional<WidgetStyle>& style) : Text(view, id, text, style) {
            m_type = "unformatted-text";
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
};

class DisabledText final : public Text {
    public:
        static std::unique_ptr<DisabledText> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view);

        DisabledText(XFrames* view, const int id, const std::string& text, std::optional<WidgetStyle>& style) : Text(view, id, text, style) {
            m_type = "disabled-text";
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
};
