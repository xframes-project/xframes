#include <algorithm>
#include "widget/table.h"
#include "xframes.h"

void Table::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    ImGui::BeginGroup();

    // ImGui::Text("Table data length: %d", (int) m_data.size());

    ImVec2 outerSize = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    if (m_clipRows > 0) {
        if (ImGui::BeginTable("t", (int)m_columns.size(), m_flags | ImGuiTableFlags_ScrollY, outerSize)) {
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
            for (const auto& columnSpec : m_columns) {
                ImGui::TableSetupColumn(columnSpec.heading.c_str(), ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
            }
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty && sort_specs->SpecsCount > 0) {
                    const auto& spec = sort_specs->Specs[0];
                    if (spec.ColumnIndex < (int)m_columns.size() && m_columns[spec.ColumnIndex].fieldId.has_value()) {
                        const auto& fieldId = m_columns[spec.ColumnIndex].fieldId.value();
                        bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
                        std::sort(m_data.begin(), m_data.end(), [&](const TableRow& a, const TableRow& b) {
                            auto itA = a.find(fieldId);
                            auto itB = b.find(fieldId);
                            if (itA == a.end()) return false;
                            if (itB == b.end()) return true;
                            return ascending ? itA->second < itB->second : itA->second > itB->second;
                        });
                    }
                    view->m_onTableSort(m_id, spec.ColumnIndex, (int)spec.SortDirection);
                    sort_specs->SpecsDirty = false;
                }
            }

            const auto numColumns = m_columns.size();

            ImGuiListClipper clipper;
            clipper.Begin((int)m_data.size());
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                {
                    ImGui::TableNextRow();
                    for (int i = 0; i < numColumns; i++) {
                        ImGui::TableSetColumnIndex(i);
                        if (m_columns[i].fieldId.has_value()) {
                            auto& fieldId = m_columns[i].fieldId.value();

                            if (m_data[row].contains(fieldId)) {
                                ImGui::TextUnformatted(m_data[row][fieldId].c_str());
                            }
                        }
                    }
                }
            }
            ImGui::EndTable();
        }
    } else if (ImGui::BeginTable("t", (int)m_columns.size(), m_flags, outerSize)) {
        for (const auto& columnSpec : m_columns) {
            ImGui::TableSetupColumn(columnSpec.heading.c_str(), ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
        }

        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty && sort_specs->SpecsCount > 0) {
                const auto& spec = sort_specs->Specs[0];
                if (spec.ColumnIndex < (int)m_columns.size() && m_columns[spec.ColumnIndex].fieldId.has_value()) {
                    const auto& fieldId = m_columns[spec.ColumnIndex].fieldId.value();
                    bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
                    std::sort(m_data.begin(), m_data.end(), [&](const TableRow& a, const TableRow& b) {
                        auto itA = a.find(fieldId);
                        auto itB = b.find(fieldId);
                        if (itA == a.end()) return false;
                        if (itB == b.end()) return true;
                        return ascending ? itA->second < itB->second : itA->second > itB->second;
                    });
                }
                view->m_onTableSort(m_id, spec.ColumnIndex, (int)spec.SortDirection);
                sort_specs->SpecsDirty = false;
            }
        }

        const auto numColumns = m_columns.size();

        for (auto& dataRow : m_data) {
            ImGui::TableNextRow();
            for (int i = 0; i < numColumns; i++) {
                ImGui::TableSetColumnIndex(i);
                if (m_columns[i].fieldId.has_value()) {
                    auto& fieldId = m_columns[i].fieldId.value();

                    if (dataRow.contains(fieldId)) {
                        ImGui::TextUnformatted(dataRow[fieldId].c_str());
                    }
                }
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
                    }
                }

                tableData.push_back(row);
            }
        }
    }

    return tableData;
}