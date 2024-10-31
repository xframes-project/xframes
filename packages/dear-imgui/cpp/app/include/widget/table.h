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
        ImGuiTableFlags m_flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

        Table(XFrames* view, const int id, const std::vector<TableColumn>& columns, const std::optional<int> clipRows, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
            m_type = "di-table";
            m_columns = columns;
            m_clipRows = 0;

            if (clipRows.has_value() && clipRows.value() > 0) {
                m_clipRows = clipRows.value();
            }
        }

    public:
        TableData m_data;
        std::vector<TableColumn> m_columns;
        int m_clipRows; // todo: potentially redundant?

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

            return makeWidget(view, id, extractedColumns, clipRows, maybeStyle);
        }

        static std::unique_ptr<Table> makeWidget(XFrames* view, const int id, const std::vector<TableColumn>& columns, std::optional<int> clipRows, std::optional<WidgetStyle>& style) {
            Table instance(view, id, columns, clipRows, style);

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