#pragma once

#include "styled_widget.h"

class XFrames;

// A table whose rows live in the application. It lays out `rowCount` rows, shows the cells of
// those inside the window [windowStart, windowStart + rows.size()) and leaves the others empty.
// It never sorts or filters: it reports the visible range, sort requests and row clicks (as
// absolute row indices) and waits for the application to send a new window.
class VirtualTable final : public StyledWidget {
    public:
        struct Column {
            std::string heading;
            ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_WidthStretch;
        };

    protected:
        VirtualTable(XFrames* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "virtual-table";
        }

    public:
        std::vector<Column> m_columns;
        std::vector<std::vector<std::string>> m_rows;
        int m_rowCount = 0;
        int m_windowStart = 0;
        int m_clipRows = 10;
        int m_selectedRow = -1;
        std::optional<int> m_scrollToRow;      // last value received
        std::optional<int> m_pendingScroll;    // applied on the next frame
        bool m_sortReported = false;           // the first sort state is ImGui's default, not a request
        int m_firstVisible = -1;               // last reported range, inclusive
        int m_lastVisible = -1;

        static std::unique_ptr<VirtualTable> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view);

        static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode);

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }

        json GetDiagnosticsState() const {
            return {{"rowCount", m_rowCount}, {"columnCount", m_columns.size()}, {"windowStart", m_windowStart},
                {"windowRows", m_rows.size()}, {"firstVisible", m_firstVisible}, {"lastVisible", m_lastVisible},
                {"selectedRow", m_selectedRow}};
        }

    private:
        // Shared by creation and patches; absent keys keep their current value.
        void Apply(const json& def);
};
