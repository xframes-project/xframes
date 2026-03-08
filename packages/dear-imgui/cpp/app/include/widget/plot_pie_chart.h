#include <cstdint>

#include <implot.h>
#include "styled_widget.h"

class PlotPieChart final : public StyledWidget {
private:
    std::vector<std::string> m_labels;
    std::vector<double> m_values;

    std::string m_labelFormat = "%.1f";
    double m_angle0 = 90;
    bool m_normalize = false;

    bool m_showLegend = true;
    int m_legendLocation = 5;

public:
    static std::unique_ptr<PlotPieChart> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();

        std::string labelFormat = "%.1f";
        if (widgetDef.contains("labelFormat") && widgetDef["labelFormat"].is_string()) {
            labelFormat = widgetDef["labelFormat"].template get<std::string>();
        }

        double angle0 = 90;
        if (widgetDef.contains("angle0")) {
            angle0 = widgetDef["angle0"].template get<double>();
        }

        bool normalize = false;
        if (widgetDef.contains("normalize")) {
            normalize = widgetDef["normalize"].template get<bool>();
        }

        bool showLegend = true;
        int legendLocation = 5;
        if (widgetDef.contains("showLegend")) {
            showLegend = widgetDef["showLegend"].template get<bool>();
        }
        if (widgetDef.contains("legendLocation")) {
            legendLocation = widgetDef["legendLocation"].template get<int>();
        }

        auto widget = std::make_unique<PlotPieChart>(view, id, labelFormat, angle0, normalize, maybeStyle);
        widget->m_showLegend = showLegend;
        widget->m_legendLocation = legendLocation;
        return widget;
    }

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    PlotPieChart(
        XFrames* view,
        const int id,
        const std::string& labelFormat,
        const double angle0,
        const bool normalize,
        std::optional<WidgetStyle>& style) : StyledWidget(view, id, style
            ) {
        m_type = "plot-pie-chart";
        m_labelFormat = labelFormat;
        m_angle0 = angle0;
        m_normalize = normalize;
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;

    void SetData(const std::vector<std::string>& labels, const std::vector<double>& values) {
        m_labels = labels;
        m_values = values;
    }

    void ResetData() {
        m_labels.clear();
        m_values.clear();
    }
};
