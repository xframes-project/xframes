#include <cstdint>

#include <implot.h>
#include "styled_widget.h"

class PlotBar final : public StyledWidget {
private:
    std::vector<double> m_xValues;
    std::vector<double> m_yValues;

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

        m_xValues.reserve(m_dataPointsLimit);
        m_yValues.reserve(m_dataPointsLimit);
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;

    void AppendData(const double x, const double y) {
        if (m_xValues.size() >= m_dataPointsLimit) {
            m_xValues.erase(m_xValues.begin());
            m_yValues.erase(m_yValues.begin());
        }

        m_xValues.push_back(x);
        m_yValues.push_back(y);
    }

    void SetData(const std::vector<double>& xs, const std::vector<double>& ys) {
        m_xValues = xs;
        m_yValues = ys;
    }

    void SetAxesAutoFit(const bool enabled) {
        m_axisAutoFit = enabled;
    }

    void ResetData() {
        m_xValues.clear();
        m_yValues.clear();
    }
};
