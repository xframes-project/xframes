#include <cstdint>

#include <implot.h>
#include "styled_widget.h"

struct PlotBarSeries {
    std::string label;
    std::vector<double> xValues;
    std::vector<double> yValues;
};

class PlotBar final : public StyledWidget {
private:
    std::vector<PlotBarSeries> m_series;

    std::vector<std::string> m_tickLabels;
    std::vector<const char*> m_tickLabelPtrs;

    int m_dataPointsLimit = 6000;

    bool m_axisAutoFit;

    std::string m_xAxisLabel;
    std::string m_yAxisLabel;

    bool m_showLegend = false;
    int m_legendLocation = 5;
    std::string m_legendLabel = "bar-plot";

public:
    static std::unique_ptr<PlotBar> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        bool axisAutoFit = false;

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
        std::string legendLabel = "bar-plot";
        if (widgetDef.contains("showLegend")) {
            showLegend = widgetDef["showLegend"].template get<bool>();
        }
        if (widgetDef.contains("legendLocation")) {
            legendLocation = widgetDef["legendLocation"].template get<int>();
        }
        if (widgetDef.contains("legendLabel") && widgetDef["legendLabel"].is_string()) {
            legendLabel = widgetDef["legendLabel"].template get<std::string>();
        }

        auto widget = std::make_unique<PlotBar>(view, id, axisAutoFit, dataPointsLimit, xAxisLabel, yAxisLabel, maybeStyle);
        widget->m_showLegend = showLegend;
        widget->m_legendLocation = legendLocation;
        widget->m_legendLabel = legendLabel;

        if (widgetDef.contains("series") && widgetDef["series"].is_array()) {
            widget->m_series.clear();
            for (auto& [key, seriesDef] : widgetDef["series"].items()) {
                PlotBarSeries s;
                s.label = seriesDef.value("label", "series-" + key);
                s.xValues.reserve(dataPointsLimit);
                s.yValues.reserve(dataPointsLimit);
                widget->m_series.push_back(std::move(s));
            }
        }

        return widget;
    }

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    PlotBar(
        XFrames* view,
        const int id,
        const bool axisAutoFit,
        const int dataPointsLimit,
        const std::string& xAxisLabel,
        const std::string& yAxisLabel,
        std::optional<WidgetStyle>& style) : StyledWidget(view, id, style
            ) {
        m_type = "plot-bar";
        m_axisAutoFit = axisAutoFit;
        m_dataPointsLimit = dataPointsLimit;
        m_xAxisLabel = xAxisLabel;
        m_yAxisLabel = yAxisLabel;

        PlotBarSeries defaultSeries;
        defaultSeries.label = m_legendLabel;
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

    void SetTickLabels(const std::vector<std::string>& labels) {
        m_tickLabels = labels;
        m_tickLabelPtrs.resize(m_tickLabels.size());
        for (size_t i = 0; i < m_tickLabels.size(); i++) {
            m_tickLabelPtrs[i] = m_tickLabels[i].c_str();
        }
    }

    void SetData(const std::vector<double>& xs, const std::vector<double>& ys) {
        auto& s = m_series[0];
        s.xValues = xs;
        s.yValues = ys;
        m_tickLabels.clear();
        m_tickLabelPtrs.clear();
    }

    void SetData(const std::vector<double>& xs, const std::vector<double>& ys, const std::vector<std::string>& tickLabels) {
        auto& s = m_series[0];
        s.xValues = xs;
        s.yValues = ys;
        SetTickLabels(tickLabels);
    }

    void SetSeriesData(const json& seriesData) {
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
            if (item.contains("tickLabels") && item["tickLabels"].is_array()) {
                std::vector<std::string> labels;
                for (auto& lbl : item["tickLabels"]) {
                    labels.push_back(lbl.template get<std::string>());
                }
                SetTickLabels(labels);
            }
        }
    }

    void SetAxesAutoFit(const bool enabled) {
        m_axisAutoFit = enabled;
    }

    void ResetData() {
        for (auto& s : m_series) {
            s.xValues.clear();
            s.yValues.clear();
        }
        m_tickLabels.clear();
        m_tickLabelPtrs.clear();
    }
};
