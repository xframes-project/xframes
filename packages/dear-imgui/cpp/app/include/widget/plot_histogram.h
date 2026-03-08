#include <cstdint>

#include <implot.h>
#include "styled_widget.h"

class PlotHistogram final : public StyledWidget {
private:
    std::vector<double> m_values;

    int m_dataPointsLimit = 6000;

    bool m_axisAutoFit;

    int m_bins = ImPlotBin_Sturges;

    std::string m_xAxisLabel;
    std::string m_yAxisLabel;

    bool m_showLegend = false;
    int m_legendLocation = 5;
    std::string m_legendLabel = "histogram";

public:
    static std::unique_ptr<PlotHistogram> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        bool axisAutoFit = false;

        if (widgetDef.contains("axisAutoFit")) {
            axisAutoFit = widgetDef["axisAutoFit"].template get<bool>();
        }

        int dataPointsLimit = 6000;
        if (widgetDef.contains("dataPointsLimit")) {
            dataPointsLimit = widgetDef["dataPointsLimit"].template get<int>();
        }

        int bins = ImPlotBin_Sturges;
        if (widgetDef.contains("bins")) {
            bins = widgetDef["bins"].template get<int>();
        }

        std::string xAxisLabel, yAxisLabel;
        if (widgetDef.contains("xAxisLabel") && widgetDef["xAxisLabel"].is_string()) {
            xAxisLabel = widgetDef["xAxisLabel"].template get<std::string>();
        }
        if (widgetDef.contains("yAxisLabel") && widgetDef["yAxisLabel"].is_string()) {
            yAxisLabel = widgetDef["yAxisLabel"].template get<std::string>();
        }

        bool showLegend = false;
        int legendLocation = 5;
        std::string legendLabel = "histogram";
        if (widgetDef.contains("showLegend")) {
            showLegend = widgetDef["showLegend"].template get<bool>();
        }
        if (widgetDef.contains("legendLocation")) {
            legendLocation = widgetDef["legendLocation"].template get<int>();
        }
        if (widgetDef.contains("legendLabel") && widgetDef["legendLabel"].is_string()) {
            legendLabel = widgetDef["legendLabel"].template get<std::string>();
        }

        auto widget = std::make_unique<PlotHistogram>(view, id, axisAutoFit, dataPointsLimit, bins, xAxisLabel, yAxisLabel, maybeStyle);
        widget->m_showLegend = showLegend;
        widget->m_legendLocation = legendLocation;
        widget->m_legendLabel = legendLabel;
        return widget;
    }

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    PlotHistogram(
        XFrames* view,
        const int id,
        const bool axisAutoFit,
        const int dataPointsLimit,
        const int bins,
        const std::string& xAxisLabel,
        const std::string& yAxisLabel,
        std::optional<WidgetStyle>& style) : StyledWidget(view, id, style
            ) {
        m_type = "plot-histogram";
        m_axisAutoFit = axisAutoFit;
        m_dataPointsLimit = dataPointsLimit;
        m_bins = bins;
        m_xAxisLabel = xAxisLabel;
        m_yAxisLabel = yAxisLabel;

        m_values.reserve(m_dataPointsLimit);
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;

    void AppendData(const double value) {
        if (m_values.size() >= m_dataPointsLimit) {
            m_values.erase(m_values.begin());
        }

        m_values.push_back(value);
    }

    void SetData(const std::vector<double>& values) {
        m_values = values;
    }

    void SetAxesAutoFit(const bool enabled) {
        m_axisAutoFit = enabled;
    }

    void ResetData() {
        m_values.clear();
    }
};
