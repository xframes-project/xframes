#include "styled_widget.h"

// todo: should we preallocate buffer size?
class ClippedMultiLineTextRenderer final : public StyledWidget {
    protected:
        ClippedMultiLineTextRenderer(XFrames* view, const int id, const int numberOfLines, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "clipped-multi-line-text-renderer";
            m_numberOfLines = numberOfLines;
        }

    public:
        int m_numberOfLines;
        ImVector<int> m_lineOffsets;
        ImGuiTextBuffer m_textBuffer;

        static std::unique_ptr<ClippedMultiLineTextRenderer> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            const auto id = widgetDef["id"].template get<int>();
            int numberOfLines = 10;

            if (widgetDef.contains("numberOfLines") && widgetDef["numberOfLines"].is_number_unsigned()) {
                numberOfLines = widgetDef["numberOfLines"].template get<int>();
            }

            return makeWidget(view, id, numberOfLines, maybeStyle);

            // throw std::invalid_argument("Invalid JSON data");
        }

        static std::unique_ptr<ClippedMultiLineTextRenderer> makeWidget(XFrames* view, int id, const int numberOfLines, std::optional<WidgetStyle>& style) {
            ClippedMultiLineTextRenderer instance(view, id, numberOfLines, style);

            return std::make_unique<ClippedMultiLineTextRenderer>(std::move(instance));
        }

        static YGSize Measure(const YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
            YGSize size{};
            const auto context = YGNodeGetContext(node);
            if (context) {
                auto widget = static_cast<ClippedMultiLineTextRenderer*>(context);

                size.width = width;
                size.height = widget->m_view->GetFrameHeight(widget) * static_cast<float>(widget->m_numberOfLines);
            }

            return size;
        }

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        void SetNumberOfLines(int numberOfLines) {
            m_numberOfLines = numberOfLines;
        }

        void Clear() {
            m_lineOffsets.clear();
            m_lineOffsets.push_back(0);
            m_textBuffer.clear();
        }

        void SetData(const char* str) {
            Clear();
            AppendText(str);
        }

        void AppendText(const char* str) {
            int old_size = m_textBuffer.size();

            m_textBuffer.append(str);

            for (const int new_size = m_textBuffer.size(); old_size < new_size; old_size++)
                if (m_textBuffer[old_size] == '\n')
                    m_lineOffsets.push_back(old_size + 1);
        }

        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }
};