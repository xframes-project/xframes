#include <algorithm>
#include <climits>
#include "widget/virtual_table.h"
#include "xframes.h"

std::unique_ptr<VirtualTable> VirtualTable::makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
    const auto id = widgetDef["id"].template get<int>();
    VirtualTable instance(view, id, maybeStyle);
    instance.Apply(widgetDef);
    return std::make_unique<VirtualTable>(std::move(instance));
}

void VirtualTable::Apply(const json& def) {
    if (def.contains("columns") && def["columns"].is_array()) {
        m_columns.clear();
        for (const auto& item : def["columns"]) {
            Column column{item["heading"].template get<std::string>()};
            if (item.contains("noSort") && item["noSort"].is_boolean() && item["noSort"].template get<bool>())
                column.flags |= ImGuiTableColumnFlags_NoSort;
            if (item.contains("widthFixed") && item["widthFixed"].is_boolean() && item["widthFixed"].template get<bool>()) {
                column.flags &= ~ImGuiTableColumnFlags_WidthStretch;
                column.flags |= ImGuiTableColumnFlags_WidthFixed;
            }
            m_columns.push_back(std::move(column));
        }
    }
    if (def.contains("rowCount") && def["rowCount"].is_number_integer()) m_rowCount = def["rowCount"].template get<int>();
    if (def.contains("windowStart") && def["windowStart"].is_number_integer()) m_windowStart = def["windowStart"].template get<int>();
    if (def.contains("rows") && def["rows"].is_array()) {
        m_rows.clear();
        m_rows.reserve(def["rows"].size());
        for (const auto& row : def["rows"]) m_rows.push_back(row.template get<std::vector<std::string>>());
    }
    if (def.contains("clipRows") && def["clipRows"].is_number_integer()) m_clipRows = std::max(1, def["clipRows"].template get<int>());
    if (def.contains("selectedRow") && def["selectedRow"].is_number_integer()) m_selectedRow = def["selectedRow"].template get<int>();
    if (def.contains("scrollToRow") && def["scrollToRow"].is_number_integer()) {
        const auto row = def["scrollToRow"].template get<int>();
        if (m_scrollToRow != row) m_pendingScroll = row;
        m_scrollToRow = row;
    }
}

void VirtualTable::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);
    Apply(widgetPatchDef);
}

YGSize VirtualTable::Measure(const YGNodeConstRef node, const float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
    YGSize size{};
    if (const auto context = YGNodeGetContext(node)) {
        const auto widget = static_cast<VirtualTable*>(context);
        const float rowHeight = widget->m_view->GetWidgetFontSize(widget) + widget->m_view->m_appStyle.CellPadding.y * 2.0f;
        size.width = width / 2;
        size.height = rowHeight * static_cast<float>(widget->m_clipRows + 1);
    }
    return size;
}

void VirtualTable::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);
    ImGui::BeginGroup();

    const ImVec2 outerSize(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;

    if (!m_columns.empty() && ImGui::BeginTable("vt", static_cast<int>(m_columns.size()), flags, outerSize)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const auto& column : m_columns) ImGui::TableSetupColumn(column.heading.c_str(), column.flags);
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
            if (m_sortReported && specs->SpecsCount > 0)
                view->m_onTableSort(m_id, specs->Specs[0].ColumnIndex, static_cast<int>(specs->Specs[0].SortDirection));
            m_sortReported = true;
            specs->SpecsDirty = false;
        }

        const float rowHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2.0f;
        if (m_pendingScroll) {
            ImGui::SetScrollY(static_cast<float>(*m_pendingScroll) * rowHeight);
            m_pendingScroll.reset();
            // The scroll takes effect on the next frame; make sure there is one.
            view->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
        }

        int first = INT_MAX;
        int last = -1;
        const int columns = static_cast<int>(m_columns.size());
        ImGuiListClipper clipper;
        clipper.Begin(m_rowCount, rowHeight);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                const int index = row - m_windowStart;
                const bool loaded = index >= 0 && index < static_cast<int>(m_rows.size());
                for (int column = 0; column < columns; column++) {
                    ImGui::TableSetColumnIndex(column);
                    if (column == 0) {
                        char label[32];
                        snprintf(label, sizeof(label), "##r%d", row);
                        if (ImGui::Selectable(label, row == m_selectedRow, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                            view->m_onTableRowClick(m_id, row);
                        }
                        if (ImGui::IsItemVisible()) {
                            first = std::min(first, row);
                            last = std::max(last, row);
                        }
                        ImGui::SameLine();
                    }
                    if (loaded && column < static_cast<int>(m_rows[index].size())) {
                        ImGui::TextUnformatted(m_rows[index][column].c_str());
                    }
                }
            }
        }

        if (last < 0) first = -1;
        if (first != m_firstVisible || last != m_lastVisible) {
            m_firstVisible = first;
            m_lastVisible = last;
            if (view->m_onTableRange) view->m_onTableRange(m_id, first, last);
        }

        ImGui::EndTable();
    }

    ImGui::EndGroup();
    ImGui::PopID();
}
