#include "styled_widget.h"

using TableRow = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;
using TableData = std::vector<TableRow>;

// todo: for those use cases where we expect large quantities of data, should we preallocate?
class Table final : public StyledWidget {
    using TableColumn = struct {
        std::optional<std::string> fieldId;
        std::string heading;
    };

    protected:
        ImGuiTableFlags m_flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Sortable;

        Table(XFrames* view, const int id, const std::vector<TableColumn>& columns, const std::optional<int> clipRows, bool filterable, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "di-table";
            m_columns = columns;
            m_clipRows = 0;
            m_filterable = filterable;

            if (m_filterable) {
                m_filters.resize(m_columns.size());
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
        std::vector<ImGuiTextFilter> m_filters;

        static std::vector<TableColumn> extractColumns(const json& columnsDef) {
            std::vector<TableColumn> columns;

            if (columnsDef.is_array()) {
                for (auto& [key, item] : columnsDef.items()) {
                    columns.push_back({
                        std::make_optional(item["fieldId"].template get<std::string>()),
                        item["heading"].template get<std::string>()
                    });
                }
            }

            return columns;
        }

        static std::unique_ptr<Table> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
            if (!widgetDef.contains("columns") || !widgetDef["columns"].is_array()) {
                throw std::invalid_argument("columns not set or not an array");
            }

            auto extractedColumns = extractColumns(widgetDef["columns"]);

            if (extractedColumns.empty()) {
                throw std::invalid_argument("no columns were extracted");
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

            return makeWidget(view, id, extractedColumns, clipRows, filterable, maybeStyle);
        }

        static std::unique_ptr<Table> makeWidget(XFrames* view, const int id, const std::vector<TableColumn>& columns, std::optional<int> clipRows, bool filterable, std::optional<WidgetStyle>& style) {
            Table instance(view, id, columns, clipRows, filterable, style);

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

            auto newColumns = extractColumns(columnsDef);
            m_columns.insert(m_columns.end(), newColumns.begin(), newColumns.end());

            if (m_filterable) {
                m_filters.clear();
                m_filters.resize(m_columns.size());
            }
        }

        void SetColumnFilter(int columnIndex, const std::string& filterText) {
            if (columnIndex >= 0 && columnIndex < (int)m_filters.size()) {
                ImStrncpy(m_filters[columnIndex].InputBuf, filterText.c_str(), IM_ARRAYSIZE(m_filters[columnIndex].InputBuf));
                m_filters[columnIndex].Build();
            }
        }

        void ClearFilters() {
            for (auto& filter : m_filters) {
                filter.Clear();
            }
        }

        bool RowPassesAllFilters(const TableRow& row) const {
            for (int i = 0; i < (int)m_columns.size(); i++) {
                if (i < (int)m_filters.size() && m_filters[i].IsActive()) {
                    if (m_columns[i].fieldId.has_value()) {
                        auto it = row.find(m_columns[i].fieldId.value());
                        if (it == row.end() || !m_filters[i].PassFilter(it->second.c_str())) {
                            return false;
                        }
                    }
                }
            }
            return true;
        }

        bool AnyFilterActive() const {
            for (const auto& filter : m_filters) {
                if (filter.IsActive()) return true;
            }
            return false;
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