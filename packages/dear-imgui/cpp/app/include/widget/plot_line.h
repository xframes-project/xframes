#include <cstdint>

#include <implot.h>
#include "styled_widget.h"

class PlotLine final : public StyledWidget {
private:
    std::vector<double> m_xValues;
    std::vector<double> m_yValues;

    int m_xAxisDecimalDigits;
    int m_yAxisDecimalDigits;

    ImPlotScale m_xAxisScale = ImPlotScale_Linear;
    ImPlotScale m_yAxisScale = ImPlotScale_Linear;

    ImPlotMarker m_markerStyle = ImPlotMarker_None;

    int m_dataPointsLimit = 6000;

    bool m_axisAutoFit;

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

        return std::make_unique<PlotLine>(view, id, xAxisDecimalDigits, yAxisDecimalDigits, markerStyle, xAxisScale, yAxisScale, axisAutoFit, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
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
        std::optional<WidgetStyle>& style) : StyledWidget(view, id, style
            ) {
        m_type = "plot-line";
        m_xAxisDecimalDigits = xAxisDecimalDigits;
        m_yAxisDecimalDigits = yAxisDecimalDigits;
        m_markerStyle = markerStyle;
        m_xAxisScale = xAxisScale;
        m_yAxisScale = yAxisScale;
        m_axisAutoFit = axisAutoFit;

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

    void SetAxesDecimalDigits(const int x, const int y) {
        m_xAxisDecimalDigits = x;
        m_yAxisDecimalDigits = y;
    }

    void SetAxesAutoFit(const bool enabled) {
        m_axisAutoFit = enabled;
    }

    void ResetData() {
        m_xValues.clear();
        m_yValues.clear();
    }
};
