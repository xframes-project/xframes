#include <cstdint>

#include <implot.h>
#include "styled_widget.h"

struct PlotLineSeries {
    std::string label;
    std::vector<double> xValues;
    std::vector<double> yValues;
    ImPlotMarker markerStyle = ImPlotMarker_None;
};

class PlotLine final : public StyledWidget {
private:
    std::vector<PlotLineSeries> m_series;

    int m_xAxisDecimalDigits;
    int m_yAxisDecimalDigits;

    ImPlotScale m_xAxisScale = ImPlotScale_Linear;
    ImPlotScale m_yAxisScale = ImPlotScale_Linear;

    ImPlotMarker m_markerStyle = ImPlotMarker_None;

    int m_dataPointsLimit = 6000;

    bool m_axisAutoFit;

    std::string m_xAxisLabel;
    std::string m_yAxisLabel;

    bool m_showLegend = false;
    int m_legendLocation = 5;
    std::string m_legendLabel = "line-plot";

public:
    static std::unique_ptr<PlotLine> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        int xAxisDecimalDigits = 0;
        int yAxisDecimalDigits = 0;
        ImPlotMarker markerStyle = ImPlotMarker_None;
        ImPlotScale xAxisScale = ImPlotScale_Linear;
        ImPlotScale yAxisScale = ImPlotScale_Linear;
        bool axisAutoFit = false;

        if (widgetDef.contains("xAxisDecimalDigits") && widgetDef.contains("yAxisDecimalDigits")) {
            xAxisDecimalDigits = widgetDef["xAxisDecimalDigits"].template get<int>();
            yAxisDecimalDigits = widgetDef["yAxisDecimalDigits"].template get<int>();
        }

        if (widgetDef.contains("markerStyle")) {
            markerStyle = widgetDef["markerStyle"].template get<int>();
        }

        if (widgetDef.contains("xAxisScale")) {
            xAxisScale = widgetDef["xAxisScale"].template get<int>();
        }

        if (widgetDef.contains("yAxisScale")) {
            yAxisScale = widgetDef["yAxisScale"].template get<int>();
        }

        if (widgetDef.contains("axisAutoFit")) {
            axisAutoFit = widgetDef["axisAutoFit"].template get<bool>();
        }

        int dataPointsLimit = 6000;
        if (widgetDef.contains("dataPointsLimit")) {
            dataPointsLimit = widgetDef["dataPointsLimit"].template get<int>();
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
        std::string legendLabel = "line-plot";
        if (widgetDef.contains("showLegend")) {
            showLegend = widgetDef["showLegend"].template get<bool>();
        }
        if (widgetDef.contains("legendLocation")) {
            legendLocation = widgetDef["legendLocation"].template get<int>();
        }
        if (widgetDef.contains("legendLabel") && widgetDef["legendLabel"].is_string()) {
            legendLabel = widgetDef["legendLabel"].template get<std::string>();
        }

        auto widget = std::make_unique<PlotLine>(view, id, xAxisDecimalDigits, yAxisDecimalDigits, markerStyle, xAxisScale, yAxisScale, axisAutoFit, dataPointsLimit, xAxisLabel, yAxisLabel, maybeStyle);
        widget->m_showLegend = showLegend;
        widget->m_legendLocation = legendLocation;
        widget->m_legendLabel = legendLabel;

        if (widgetDef.contains("series") && widgetDef["series"].is_array()) {
            widget->m_series.clear();
            for (auto& [key, seriesDef] : widgetDef["series"].items()) {
                PlotLineSeries s;
                s.label = seriesDef.value("label", "series-" + key);
                s.markerStyle = seriesDef.value("markerStyle", ImPlotMarker_None);
                s.xValues.reserve(dataPointsLimit);
                s.yValues.reserve(dataPointsLimit);
                widget->m_series.push_back(std::move(s));
            }
        }

        return widget;
    }

    static int axisValueFormatter(double value, char* buff, int size, void* decimalPlaces) {
        if (value == 0) {
            return snprintf(buff, size,"0");
        }

        return snprintf(buff, size, "%.*f", reinterpret_cast<intptr_t>(decimalPlaces), value);
    };

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    PlotLine(
        XFrames* view,
        const int id,
        const int xAxisDecimalDigits,
        const int yAxisDecimalDigits,
        const ImPlotMarker markerStyle,
        const ImPlotScale xAxisScale,
        const ImPlotScale yAxisScale,
        const bool axisAutoFit,
        const int dataPointsLimit,
        const std::string& xAxisLabel,
        const std::string& yAxisLabel,
        std::optional<WidgetStyle>& style) : StyledWidget(view, id, style
            ) {
        m_type = "plot-line";
        m_xAxisDecimalDigits = xAxisDecimalDigits;
        m_yAxisDecimalDigits = yAxisDecimalDigits;
        m_markerStyle = markerStyle;
        m_xAxisScale = xAxisScale;
        m_yAxisScale = yAxisScale;
        m_axisAutoFit = axisAutoFit;
        m_dataPointsLimit = dataPointsLimit;
        m_xAxisLabel = xAxisLabel;
        m_yAxisLabel = yAxisLabel;

        PlotLineSeries defaultSeries;
        defaultSeries.label = m_legendLabel;
        defaultSeries.markerStyle = m_markerStyle;
        defaultSeries.xValues.reserve(m_dataPointsLimit);
        defaultSeries.yValues.reserve(m_dataPointsLimit);
        m_series.push_back(std::move(defaultSeries));
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;

    void AppendData(const double x, const double y) {
        AppendSeriesData(0, x, y);
    }

    void AppendSeriesData(const int seriesIndex, const double x, const double y) {
        if (seriesIndex < 0 || seriesIndex >= static_cast<int>(m_series.size())) return;
        auto& s = m_series[seriesIndex];
        if (static_cast<int>(s.xValues.size()) >= m_dataPointsLimit) {
            s.xValues.erase(s.xValues.begin());
            s.yValues.erase(s.yValues.begin());
        }
        s.xValues.push_back(x);
        s.yValues.push_back(y);
    }

    void SetData(const json& seriesData) {
        for (auto& [key, item] : seriesData.items()) {
            int idx = std::stoi(key);
            if (idx < 0 || idx >= static_cast<int>(m_series.size())) continue;
            auto& s = m_series[idx];
            s.xValues.clear();
            s.yValues.clear();
            if (item.contains("data") && item["data"].is_array()) {
                for (auto& [dkey, d] : item["data"].items()) {
                    if (d.is_object()) {
                        s.xValues.push_back(d["x"].template get<double>());
                        s.yValues.push_back(d["y"].template get<double>());
                    }
                }
            }
        }
    }

    void SetAxesDecimalDigits(const int x, const int y) {
        m_xAxisDecimalDigits = x;
        m_yAxisDecimalDigits = y;
    }

    void SetAxesAutoFit(const bool enabled) {
        m_axisAutoFit = enabled;
    }

    void ResetData() {
        for (auto& s : m_series) {
            s.xValues.clear();
            s.yValues.clear();
        }
    }
};
