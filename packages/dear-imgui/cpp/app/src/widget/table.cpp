#include <algorithm>
#include <cmath>
#include "widget/table.h"
#include "xframes.h"

void Table::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    ImGui::BeginGroup();

    ImVec2 outerSize = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    if (m_clipRows > 0) {
        if (ImGui::BeginTable("t", (int)m_columns.size(), m_flags | ImGuiTableFlags_ScrollY, outerSize)) {
            int frozenRows = m_filterable ? 2 : 1;
            ImGui::TableSetupScrollFreeze(0, frozenRows);
            for (const auto& columnSpec : m_columns) {
                ImGui::TableSetupColumn(columnSpec.heading.c_str(), columnSpec.flags);
            }
            ImGui::TableHeadersRow();

            // Filter row
            if (m_filterable) {
                ImGui::TableNextRow();
                for (int i = 0; i < (int)m_columns.size(); i++) {
                    ImGui::TableSetColumnIndex(i);
                    ImGui::PushID(i);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (m_columns[i].type == ColumnType::Boolean) {
                        const char* boolOptions[] = { "All", "Yes", "No" };
                        if (ImGui::Combo("##filter", &m_boolFilterStates[i], boolOptions, 3)) {
                            m_filterDirty = true;
                            const char* labels[] = { "", "Yes", "No" };
                            view->m_onTableFilter(m_id, i, std::string(labels[m_boolFilterStates[i]]));
                        }
                    } else {
                        if (m_filters[i].Draw("##filter")) {
                            m_filterDirty = true;
                            view->m_onTableFilter(m_id, i, std::string(m_filters[i].InputBuf));
                        }
                    }
                    ImGui::PopID();
                }
            }

            if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty && sort_specs->SpecsCount > 0) {
                    const auto& spec = sort_specs->Specs[0];
                    if (spec.ColumnIndex < (int)m_columns.size() && m_columns[spec.ColumnIndex].fieldId.has_value()) {
                        const auto& fieldId = m_columns[spec.ColumnIndex].fieldId.value();
                        const auto& colType = m_columns[spec.ColumnIndex].type;
                        m_filterDirty = true;
                        bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
                        std::sort(m_data.begin(), m_data.end(), [&](const TableRow& a, const TableRow& b) {
                            auto itA = a.find(fieldId);
                            auto itB = b.find(fieldId);
                            if (itA == a.end()) return false;
                            if (itB == b.end()) return true;
                            if (colType == ColumnType::Number) {
                                try {
                                    double numA = std::stod(itA->second);
                                    double numB = std::stod(itB->second);
                                    return ascending ? numA < numB : numA > numB;
                                } catch (...) {
                                    return ascending ? itA->second < itB->second : itA->second > itB->second;
                                }
                            }
                            return ascending ? itA->second < itB->second : itA->second > itB->second;
                        });
                    }
                    view->m_onTableSort(m_id, spec.ColumnIndex, (int)spec.SortDirection);
                    sort_specs->SpecsDirty = false;
                }
            }

            const auto numColumns = m_columns.size();

            bool openContextMenu = false;

            if (m_filterable && AnyFilterActive()) {
                // Rebuild filtered index list only when data or filters changed
                if (m_filterDirty) {
                    m_filteredIndices.clear();
                    m_filteredIndices.reserve(m_data.size());
                    for (int r = 0; r < (int)m_data.size(); r++) {
                        if (RowPassesAllFilters(m_data[r])) {
                            m_filteredIndices.push_back(r);
                        }
                    }
                    m_filterDirty = false;
                }

                ImGuiListClipper clipper;
                clipper.Begin((int)m_filteredIndices.size());
                while (clipper.Step()) {
                    for (int fi = clipper.DisplayStart; fi < clipper.DisplayEnd; fi++) {
                        int row = m_filteredIndices[fi];
                        ImGui::TableNextRow();
                        for (int i = 0; i < (int)numColumns; i++) {
                            ImGui::TableSetColumnIndex(i);
                            if (i == 0) {
                                bool isSelected = (m_selectedRowIndex == row);
                                char label[32];
                                snprintf(label, sizeof(label), "##row%d", row);
                                if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                                    m_selectedRowIndex = row;
                                    view->m_onTableRowClick(m_id, row);
                                }
                                if (!m_contextMenuItems.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                                    m_contextMenuRowIndex = row;
                                    openContextMenu = true;
                                }
                                ImGui::SameLine();
                            }
                            if (m_columns[i].fieldId.has_value()) {
                                auto& fieldId = m_columns[i].fieldId.value();
                                auto cellIt = m_data[row].find(fieldId);
                                if (cellIt != m_data[row].end()) {
                                    RenderCell(cellIt->second, m_columns[i].type);
                                }
                            }
                        }
                    }
                }
            } else {
                ImGuiListClipper clipper;
                clipper.Begin((int)m_data.size());
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                        ImGui::TableNextRow();
                        for (int i = 0; i < (int)numColumns; i++) {
                            ImGui::TableSetColumnIndex(i);
                            if (i == 0) {
                                bool isSelected = (m_selectedRowIndex == row);
                                char label[32];
                                snprintf(label, sizeof(label), "##row%d", row);
                                if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                                    m_selectedRowIndex = row;
                                    view->m_onTableRowClick(m_id, row);
                                }
                                if (!m_contextMenuItems.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                                    m_contextMenuRowIndex = row;
                                    openContextMenu = true;
                                }
                                ImGui::SameLine();
                            }
                            if (m_columns[i].fieldId.has_value()) {
                                auto& fieldId = m_columns[i].fieldId.value();
                                auto cellIt = m_data[row].find(fieldId);
                                if (cellIt != m_data[row].end()) {
                                    RenderCell(cellIt->second, m_columns[i].type);
                                }
                            }
                        }
                    }
                }
            }

            if (!m_contextMenuItems.empty()) {
                if (openContextMenu) {
                    ImGui::OpenPopup("##table_ctx");
                }
                if (ImGui::BeginPopup("##table_ctx")) {
                    for (const auto& item : m_contextMenuItems) {
                        if (ImGui::MenuItem(item.label.c_str())) {
                            view->m_onTableItemAction(m_id, m_contextMenuRowIndex, item.id);
                        }
                    }
                    ImGui::EndPopup();
                }
            }

            ImGui::EndTable();
        }
    } else if (ImGui::BeginTable("t", (int)m_columns.size(), m_flags, outerSize)) {
        for (const auto& columnSpec : m_columns) {
            ImGui::TableSetupColumn(columnSpec.heading.c_str(), columnSpec.flags);
        }

        ImGui::TableHeadersRow();

        // Filter row
        if (m_filterable) {
            ImGui::TableNextRow();
            for (int i = 0; i < (int)m_columns.size(); i++) {
                ImGui::TableSetColumnIndex(i);
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (m_columns[i].type == ColumnType::Boolean) {
                    const char* boolOptions[] = { "All", "Yes", "No" };
                    if (ImGui::Combo("##filter", &m_boolFilterStates[i], boolOptions, 3)) {
                        m_filterDirty = true;
                        const char* labels[] = { "", "Yes", "No" };
                        view->m_onTableFilter(m_id, i, std::string(labels[m_boolFilterStates[i]]));
                    }
                } else {
                    if (m_filters[i].Draw("##filter")) {
                        m_filterDirty = true;
                        view->m_onTableFilter(m_id, i, std::string(m_filters[i].InputBuf));
                    }
                }
                ImGui::PopID();
            }
        }

        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty && sort_specs->SpecsCount > 0) {
                const auto& spec = sort_specs->Specs[0];
                if (spec.ColumnIndex < (int)m_columns.size() && m_columns[spec.ColumnIndex].fieldId.has_value()) {
                    const auto& fieldId = m_columns[spec.ColumnIndex].fieldId.value();
                    const auto& colType = m_columns[spec.ColumnIndex].type;
                    m_filterDirty = true;
                    bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
                    std::sort(m_data.begin(), m_data.end(), [&](const TableRow& a, const TableRow& b) {
                        auto itA = a.find(fieldId);
                        auto itB = b.find(fieldId);
                        if (itA == a.end()) return false;
                        if (itB == b.end()) return true;
                        if (colType == ColumnType::Number) {
                            try {
                                double numA = std::stod(itA->second);
                                double numB = std::stod(itB->second);
                                return ascending ? numA < numB : numA > numB;
                            } catch (...) {
                                return ascending ? itA->second < itB->second : itA->second > itB->second;
                            }
                        }
                        return ascending ? itA->second < itB->second : itA->second > itB->second;
                    });
                }
                view->m_onTableSort(m_id, spec.ColumnIndex, (int)spec.SortDirection);
                sort_specs->SpecsDirty = false;
            }
        }

        const auto numColumns = m_columns.size();
        bool openContextMenu = false;

        for (int row = 0; row < (int)m_data.size(); row++) {
            auto& dataRow = m_data[row];
            if (m_filterable && !RowPassesAllFilters(dataRow)) continue;

            ImGui::TableNextRow();
            for (int i = 0; i < (int)numColumns; i++) {
                ImGui::TableSetColumnIndex(i);
                if (i == 0) {
                    bool isSelected = (m_selectedRowIndex == row);
                    char label[32];
                    snprintf(label, sizeof(label), "##row%d", row);
                    if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                        m_selectedRowIndex = row;
                        view->m_onTableRowClick(m_id, row);
                    }
                    if (!m_contextMenuItems.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        m_contextMenuRowIndex = row;
                        openContextMenu = true;
                    }
                    ImGui::SameLine();
                }
                if (m_columns[i].fieldId.has_value()) {
                    auto& fieldId = m_columns[i].fieldId.value();

                    if (dataRow.contains(fieldId)) {
                        RenderCell(dataRow[fieldId], m_columns[i].type);
                    }
                }
            }
        }

        if (!m_contextMenuItems.empty()) {
            if (openContextMenu) {
                ImGui::OpenPopup("##table_ctx");
            }
            if (ImGui::BeginPopup("##table_ctx")) {
                for (const auto& item : m_contextMenuItems) {
                    if (ImGui::MenuItem(item.label.c_str())) {
                        view->m_onTableItemAction(m_id, m_contextMenuRowIndex, item.id);
                    }
                }
                ImGui::EndPopup();
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndGroup();
    ImGui::PopID();
};

void Table::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("columns") && widgetPatchDef["columns"].is_array()) {
        SetColumns(widgetPatchDef["columns"]);
    }

    if (widgetPatchDef.contains("filterable") && widgetPatchDef["filterable"].is_boolean()) {
        m_filterable = widgetPatchDef["filterable"].template get<bool>();
        if (m_filterable && m_filters.size() != m_columns.size()) {
            m_filters.resize(m_columns.size());
            m_boolFilterStates.resize(m_columns.size(), 0);
        }
    }

    if (widgetPatchDef.contains("reorderable") && widgetPatchDef["reorderable"].is_boolean()) {
        if (widgetPatchDef["reorderable"].template get<bool>()) {
            m_flags |= ImGuiTableFlags_Reorderable;
        } else {
            m_flags &= ~ImGuiTableFlags_Reorderable;
        }
    }

    if (widgetPatchDef.contains("contextMenuItems") && widgetPatchDef["contextMenuItems"].is_array()) {
        m_contextMenuItems.clear();
        for (const auto& item : widgetPatchDef["contextMenuItems"]) {
            if (item.is_object() && item.contains("id") && item.contains("label")) {
                m_contextMenuItems.push_back({
                    item["id"].template get<std::string>(),
                    item["label"].template get<std::string>()
                });
            }
        }
    }

    if (widgetPatchDef.contains("hideable") && widgetPatchDef["hideable"].is_boolean()) {
        m_hideable = widgetPatchDef["hideable"].template get<bool>();
        if (m_hideable) {
            m_flags |= ImGuiTableFlags_Hideable;
        } else {
            m_flags &= ~ImGuiTableFlags_Hideable;
        }
    }
};

bool Table::HasInternalOps() {
    return true;
}

void Table::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        auto op = opDef["op"].template get<std::string>();

        if (op == "appendData" && opDef.contains("data") && opDef["data"].is_array()) {
            auto tableData = parseTableData(opDef["data"]);

            AppendData(tableData);
        } else if (op == "setData" && opDef.contains("data") && opDef["data"].is_array()) {
            auto tableData = parseTableData(opDef["data"]);

            SetData(tableData);
        } else if (op == "setColumnFilter" && opDef.contains("columnIndex") && opDef.contains("filterText")) {
            int columnIndex = opDef["columnIndex"].template get<int>();
            auto filterText = opDef["filterText"].template get<std::string>();
            SetColumnFilter(columnIndex, filterText);
        } else if (op == "clearFilters") {
            ClearFilters();
        }
    }
};

YGSize Table::Measure(const YGNodeConstRef node, const float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
    YGSize size{};
    const auto context = YGNodeGetContext(node);
    if (context) {
        const auto widget = static_cast<Table*>(context);
        const float fontSize = widget->m_view->GetWidgetFontSize(widget);

        size.width = width / 2;
        if (widget->m_clipRows > 0) {
            size.height = fontSize * static_cast<float>(widget->m_clipRows);
        } else {
            size.height = fontSize * 5;
        }
    }

    return size;
};

TableData Table::parseTableData(const json& jsonTableData) {
    auto tableData = TableData();

    if (jsonTableData.is_array()) {
        for (auto& [parsedItemKey, parsedRow] : jsonTableData.items()) {
            if (parsedRow.is_object()) {
                auto row = TableRow();

                for (auto& [parsedRowFieldKey, parsedRowFieldValue] : parsedRow.items()) {
                    if (parsedRowFieldValue.is_string()) {
                        row[parsedRowFieldKey] = parsedRowFieldValue.template get<std::string>();
                    } else if (parsedRowFieldValue.is_number()) {
                        row[parsedRowFieldKey] = std::to_string(parsedRowFieldValue.template get<double>());
                    } else if (parsedRowFieldValue.is_boolean()) {
                        row[parsedRowFieldKey] = parsedRowFieldValue.template get<bool>() ? "true" : "false";
                    }
                }

                tableData.push_back(row);
            }
        }
    }

    return tableData;
}