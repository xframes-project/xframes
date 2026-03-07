#include "styled_widget.h"

using TableRow = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;
using TableData = std::vector<TableRow>;

// todo: for those use cases where we expect large quantities of data, should we preallocate?
class Table final : public StyledWidget {
    struct TableColumn {
        std::optional<std::string> fieldId;
        std::string heading;
        ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch;
        std::string type = "string"; // "string", "number", "boolean"
    };

    protected:
        ImGuiTableFlags m_flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Sortable;

        Table(XFrames* view, const int id, const std::vector<TableColumn>& columns, const std::optional<int> clipRows, bool filterable, bool reorderable, bool hideable, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "di-table";
            m_columns = columns;
            m_clipRows = 0;
            m_filterable = filterable;
            m_hideable = hideable;

            if (m_filterable) {
                m_filters.resize(m_columns.size());
                m_boolFilterStates.resize(m_columns.size(), 0);
            }

            if (reorderable) {
                m_flags |= ImGuiTableFlags_Reorderable;
            }
            if (hideable) {
                m_flags |= ImGuiTableFlags_Hideable;
            }

            if (clipRows.has_value() && clipRows.value() > 0) {
                m_clipRows = clipRows.value();
            }
        }

    public:
        TableData m_data;
        std::vector<TableColumn> m_columns;
        int m_clipRows; // todo: potentially redundant?
        bool m_filterable = false;
        bool m_hideable = false;
        int m_selectedRowIndex = -1;
        std::vector<ImGuiTextFilter> m_filters;
        std::vector<int> m_boolFilterStates; // 0=All, 1=Yes, 2=No

        static std::vector<TableColumn> extractColumns(const json& columnsDef, bool tableHideable = false) {
            std::vector<TableColumn> columns;

            if (columnsDef.is_array()) {
                for (auto& [key, item] : columnsDef.items()) {
                    ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_WidthStretch;

                    // When table is not hideable, default to NoHide (original behavior)
                    if (!tableHideable) {
                        flags |= ImGuiTableColumnFlags_NoHide;
                    }

                    // Per-column flag overrides
                    if (item.contains("defaultHide") && item["defaultHide"].is_boolean() && item["defaultHide"].template get<bool>()) {
                        flags |= ImGuiTableColumnFlags_DefaultHide;
                    }
                    if (item.contains("defaultSort") && item["defaultSort"].is_boolean() && item["defaultSort"].template get<bool>()) {
                        flags |= ImGuiTableColumnFlags_DefaultSort;
                    }
                    if (item.contains("widthFixed") && item["widthFixed"].is_boolean() && item["widthFixed"].template get<bool>()) {
                        flags &= ~ImGuiTableColumnFlags_WidthStretch;
                        flags |= ImGuiTableColumnFlags_WidthFixed;
                    }
                    if (item.contains("noSort") && item["noSort"].is_boolean() && item["noSort"].template get<bool>()) {
                        flags |= ImGuiTableColumnFlags_NoSort;
                    }
                    if (item.contains("noResize") && item["noResize"].is_boolean() && item["noResize"].template get<bool>()) {
                        flags |= ImGuiTableColumnFlags_NoResize;
                    }
                    if (item.contains("noReorder") && item["noReorder"].is_boolean() && item["noReorder"].template get<bool>()) {
                        flags |= ImGuiTableColumnFlags_NoReorder;
                    }
                    if (item.contains("noHide") && item["noHide"].is_boolean() && item["noHide"].template get<bool>()) {
                        flags |= ImGuiTableColumnFlags_NoHide;
                    }

                    std::string colType = "string";
                    if (item.contains("type") && item["type"].is_string()) {
                        colType = item["type"].template get<std::string>();
                    }

                    columns.push_back({
                        std::make_optional(item["fieldId"].template get<std::string>()),
                        item["heading"].template get<std::string>(),
                        flags,
                        colType
                    });
                }
            }

            return columns;
        }

        static std::unique_ptr<Table> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            if (!widgetDef.contains("columns") || !widgetDef["columns"].is_array()) {
                throw std::invalid_argument("columns not set or not an array");
            }

            const auto id = widgetDef["id"].template get<int>();
            std::optional<int> clipRows;

            if (widgetDef.contains("clipRows") && widgetDef["clipRows"].is_number_integer()) {
                clipRows.emplace(widgetDef["clipRows"].template get<int>());
            }

            bool filterable = false;
            if (widgetDef.contains("filterable") && widgetDef["filterable"].is_boolean()) {
                filterable = widgetDef["filterable"].template get<bool>();
            }

            bool reorderable = false;
            if (widgetDef.contains("reorderable") && widgetDef["reorderable"].is_boolean()) {
                reorderable = widgetDef["reorderable"].template get<bool>();
            }

            bool hideable = false;
            if (widgetDef.contains("hideable") && widgetDef["hideable"].is_boolean()) {
                hideable = widgetDef["hideable"].template get<bool>();
            }

            auto extractedColumns = extractColumns(widgetDef["columns"], hideable);

            if (extractedColumns.empty()) {
                throw std::invalid_argument("no columns were extracted");
            }

            return makeWidget(view, id, extractedColumns, clipRows, filterable, reorderable, hideable, maybeStyle);
        }

        static std::unique_ptr<Table> makeWidget(XFrames* view, const int id, const std::vector<TableColumn>& columns, std::optional<int> clipRows, bool filterable, bool reorderable, bool hideable, std::optional<WidgetStyle>& style) {
            Table instance(view, id, columns, clipRows, filterable, reorderable, hideable, style);

            return std::make_unique<Table>(std::move(instance));
        }

        static YGSize Measure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode);

        static TableData parseTableData(const json& jsonTableData);

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void Patch(const json& widgetPatchDef, XFrames* view) override;

        bool HasInternalOps();

        void HandleInternalOp(const json& opDef);

        void SetColumns(const json& columnsDef) {
            m_data.clear();
            m_columns.clear();

            auto newColumns = extractColumns(columnsDef, m_hideable);
            m_columns.insert(m_columns.end(), newColumns.begin(), newColumns.end());

            if (m_filterable) {
                m_filters.clear();
                m_filters.resize(m_columns.size());
                m_boolFilterStates.clear();
                m_boolFilterStates.resize(m_columns.size(), 0);
            }
        }

        void SetColumnFilter(int columnIndex, const std::string& filterText) {
            if (columnIndex >= 0 && columnIndex < (int)m_columns.size()) {
                if (m_columns[columnIndex].type == "boolean") {
                    if (columnIndex < (int)m_boolFilterStates.size()) {
                        if (filterText == "Yes") m_boolFilterStates[columnIndex] = 1;
                        else if (filterText == "No") m_boolFilterStates[columnIndex] = 2;
                        else m_boolFilterStates[columnIndex] = 0;
                    }
                } else if (columnIndex < (int)m_filters.size()) {
                    ImStrncpy(m_filters[columnIndex].InputBuf, filterText.c_str(), IM_ARRAYSIZE(m_filters[columnIndex].InputBuf));
                    m_filters[columnIndex].Build();
                }
            }
        }

        void ClearFilters() {
            for (auto& filter : m_filters) {
                filter.Clear();
            }
            std::fill(m_boolFilterStates.begin(), m_boolFilterStates.end(), 0);
        }

        bool RowPassesAllFilters(const TableRow& row) const {
            for (int i = 0; i < (int)m_columns.size(); i++) {
                if (!m_columns[i].fieldId.has_value()) continue;

                if (m_columns[i].type == "boolean") {
                    if (i < (int)m_boolFilterStates.size() && m_boolFilterStates[i] != 0) {
                        auto it = row.find(m_columns[i].fieldId.value());
                        if (it == row.end()) return false;
                        bool isTrue = (it->second == "true" || it->second == "1");
                        if (m_boolFilterStates[i] == 1 && !isTrue) return false;
                        if (m_boolFilterStates[i] == 2 && isTrue) return false;
                    }
                } else if (i < (int)m_filters.size() && m_filters[i].IsActive()) {
                    auto it = row.find(m_columns[i].fieldId.value());
                    if (it == row.end()) return false;
                    std::string display = GetDisplayValue(it->second, m_columns[i].type);
                    if (!m_filters[i].PassFilter(display.c_str())) {
                        return false;
                    }
                }
            }
            return true;
        }

        bool AnyFilterActive() const {
            for (const auto& filter : m_filters) {
                if (filter.IsActive()) return true;
            }
            for (const auto& state : m_boolFilterStates) {
                if (state != 0) return true;
            }
            return false;
        }

        std::string GetDisplayValue(const std::string& value, const std::string& colType) const {
            if (colType == "number") {
                try {
                    double num = std::stod(value);
                    char buf[64];
                    if (num == std::floor(num) && std::abs(num) < 1e15) {
                        snprintf(buf, sizeof(buf), "%lld", (long long)num);
                    } else {
                        snprintf(buf, sizeof(buf), "%.2f", num);
                    }
                    return buf;
                } catch (...) { return value; }
            }
            return value;
        }

        void RenderCell(const std::string& value, const std::string& colType) {
            if (colType == "number") {
                try {
                    double num = std::stod(value);
                    // Check if it's a whole number
                    if (num == std::floor(num) && std::abs(num) < 1e15) {
                        ImGui::Text("%lld", (long long)num);
                    } else {
                        ImGui::Text("%.2f", num);
                    }
                } catch (...) {
                    ImGui::TextUnformatted(value.c_str());
                }
            } else if (colType == "boolean") {
                if (value == "true" || value == "1") {
                    ImGui::TextUnformatted("Yes");
                } else {
                    ImGui::TextUnformatted("No");
                }
            } else {
                ImGui::TextUnformatted(value.c_str());
            }
        }

        void SetClipRows(int clipRows) {
            if (clipRows > 0) {
                m_clipRows = clipRows;
            }
        }

        void SetData(TableData& data) {
            m_data.clear();
            AppendData(data);
        }

        void AppendData(TableData& data) {
            m_data.insert(m_data.end(), data.begin(), data.end());
        }

        // TODO: this is repeated a million times
        void Init(const json& elementDef) override {
            Element::Init(elementDef);

            YGNodeSetContext(m_layoutNode->m_node, this);
            YGNodeSetMeasureFunc(m_layoutNode->m_node, Measure);
        }
};