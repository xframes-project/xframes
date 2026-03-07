#include <cstdint>

#include <implot.h>
#include "styled_widget.h"

class PlotHeatmap final : public StyledWidget {
private:
    std::vector<double> m_values;
    int m_rows = 0;
    int m_cols = 0;

    double m_scaleMin = 0;
    double m_scaleMax = 0;

    ImPlotColormap m_colormap = ImPlotColormap_Viridis;

    bool m_axisAutoFit;

    std::string m_xAxisLabel;
    std::string m_yAxisLabel;

public:
    static std::unique_ptr<PlotHeatmap> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        bool axisAutoFit = false;
        double scaleMin = 0;
        double scaleMax = 0;
        ImPlotColormap colormap = ImPlotColormap_Viridis;

        if (widgetDef.contains("axisAutoFit")) {
            axisAutoFit = widgetDef["axisAutoFit"].template get<bool>();
        }

        if (widgetDef.contains("scaleMin")) {
            scaleMin = widgetDef["scaleMin"].template get<double>();
        }

        if (widgetDef.contains("scaleMax")) {
            scaleMax = widgetDef["scaleMax"].template get<double>();
        }

        if (widgetDef.contains("colormap")) {
            colormap = widgetDef["colormap"].template get<int>();
        }

        std::string xAxisLabel, yAxisLabel;
        if (widgetDef.contains("xAxisLabel") && widgetDef["xAxisLabel"].is_string()) {
            xAxisLabel = widgetDef["xAxisLabel"].template get<std::string>();
        }
        if (widgetDef.contains("yAxisLabel") && widgetDef["yAxisLabel"].is_string()) {
            yAxisLabel = widgetDef["yAxisLabel"].template get<std::string>();
        }

        return std::make_unique<PlotHeatmap>(view, id, axisAutoFit, scaleMin, scaleMax, colormap, xAxisLabel, yAxisLabel, maybeStyle);
    }

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    PlotHeatmap(
        XFrames* view,
        const int id,
        const bool axisAutoFit,
        const double scaleMin,
        const double scaleMax,
        const ImPlotColormap colormap,
        const std::string& xAxisLabel,
        const std::string& yAxisLabel,
        std::optional<WidgetStyle>& style) : StyledWidget(view, id, style
            ) {
        m_type = "plot-heatmap";
        m_axisAutoFit = axisAutoFit;
        m_scaleMin = scaleMin;
        m_scaleMax = scaleMax;
        m_colormap = colormap;
        m_xAxisLabel = xAxisLabel;
        m_yAxisLabel = yAxisLabel;
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;

    void SetData(const int rows, const int cols, const std::vector<double>& values) {
        m_rows = rows;
        m_cols = cols;
        m_values = values;
    }

    void SetAxesAutoFit(const bool enabled) {
        m_axisAutoFit = enabled;
    }

    void ResetData() {
        m_values.clear();
        m_rows = 0;
        m_cols = 0;
    }
};
