#include <imgui.h>

#include "widget/plot_line.h"
#include "xframes.h"

bool PlotLine::HasCustomWidth() {
    return false;
}

bool PlotLine::HasCustomHeight() {
    return false;
}

void PlotLine::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    auto size = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    ImPlotFlags plotFlags = ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoTitle;
    if (!m_showLegend) plotFlags |= ImPlotFlags_NoLegend;

    if (ImPlot::BeginPlot("plot_line", size, plotFlags)) {
        if (m_showLegend) {
            ImPlot::SetupLegend(static_cast<ImPlotLocation>(m_legendLocation));
        }
        const char* xLabel = m_xAxisLabel.empty() ? nullptr : m_xAxisLabel.c_str();
        const char* yLabel = m_yAxisLabel.empty() ? nullptr : m_yAxisLabel.c_str();

        if (m_axisAutoFit) {
            ImPlot::SetupAxes(xLabel, yLabel, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        } else {
            ImPlot::SetupAxes(xLabel, yLabel);
        }

        ImPlot::SetupAxisScale(ImAxis_X1, m_xAxisScale);
        ImPlot::SetupAxisScale(ImAxis_Y1, m_yAxisScale);

        if (m_xAxisDecimalDigits > 0) {
            ImPlot::SetupAxisFormat(ImAxis_X1, axisValueFormatter, (void*)m_xAxisDecimalDigits);
        }

        if (m_yAxisDecimalDigits > 0) {
            ImPlot::SetupAxisFormat(ImAxis_Y1, axisValueFormatter, (void*)m_yAxisDecimalDigits);
        }

        for (const auto& series : m_series) {
            if (series.xValues.empty()) continue;
            ImPlot::SetNextMarkerStyle(series.markerStyle);
            ImPlot::PlotLine(
                series.label.c_str(),
                series.xValues.data(),
                series.yValues.data(),
                static_cast<int>(series.xValues.size())
            );
        }

        ImPlot::EndPlot();
    }

    ImGui::PopID();
};

void PlotLine::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("xAxisDecimalDigits") && widgetPatchDef.contains("yAxisDecimalDigits")) {
        const auto xAxisDecimalDigits = widgetPatchDef["xAxisDecimalDigits"].template get<int>();
        const auto yAxisDecimalDigits = widgetPatchDef["yAxisDecimalDigits"].template get<int>();

        SetAxesDecimalDigits(xAxisDecimalDigits, yAxisDecimalDigits);
    }

    if (widgetPatchDef.contains("axisAutoFit")) {
        const auto axisAutoFit = widgetPatchDef["axisAutoFit"].template get<bool>();
        SetAxesAutoFit(axisAutoFit);
    }

    if (widgetPatchDef.contains("xAxisLabel") && widgetPatchDef["xAxisLabel"].is_string()) {
        m_xAxisLabel = widgetPatchDef["xAxisLabel"].template get<std::string>();
    }
    if (widgetPatchDef.contains("yAxisLabel") && widgetPatchDef["yAxisLabel"].is_string()) {
        m_yAxisLabel = widgetPatchDef["yAxisLabel"].template get<std::string>();
    }

    if (widgetPatchDef.contains("showLegend")) {
        m_showLegend = widgetPatchDef["showLegend"].template get<bool>();
    }
    if (widgetPatchDef.contains("legendLocation")) {
        m_legendLocation = widgetPatchDef["legendLocation"].template get<int>();
    }
    if (widgetPatchDef.contains("legendLabel") && widgetPatchDef["legendLabel"].is_string()) {
        m_legendLabel = widgetPatchDef["legendLabel"].template get<std::string>();
    }
    if (widgetPatchDef.contains("markerStyle")) {
        m_markerStyle = widgetPatchDef["markerStyle"].template get<int>();
    }

    if (widgetPatchDef.contains("series") && widgetPatchDef["series"].is_array()) {
        const auto& seriesDef = widgetPatchDef["series"];
        size_t newCount = seriesDef.size();

        if (newCount > m_series.size()) {
            for (size_t i = m_series.size(); i < newCount; i++) {
                PlotLineSeries s;
                s.xValues.reserve(m_dataPointsLimit);
                s.yValues.reserve(m_dataPointsLimit);
                m_series.push_back(std::move(s));
            }
        } else if (newCount < m_series.size()) {
            m_series.resize(newCount);
        }

        for (size_t i = 0; i < newCount; i++) {
            const auto& item = seriesDef[i];
            if (item.contains("label") && item["label"].is_string()) {
                m_series[i].label = item["label"].template get<std::string>();
            }
            if (item.contains("markerStyle")) {
                m_series[i].markerStyle = item["markerStyle"].template get<int>();
            }
        }
    }

    // Sync single-series backward compat
    if (!widgetPatchDef.contains("series") && m_series.size() == 1) {
        m_series[0].label = m_legendLabel;
        m_series[0].markerStyle = m_markerStyle;
    }
};

bool PlotLine::HasInternalOps() {
    return true;
}

// void XFrames::RenderMap(int id, double centerX, double centerY, int zoom)
void PlotLine::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        const auto op = opDef["op"].template get<std::string>();

        if (op == "appendData" && opDef.contains("x") && opDef.contains("y")) {
            const auto x = opDef["x"].template get<double>();
            const auto y = opDef["y"].template get<double>();

            AppendData(x, y);
        } else if (op == "appendSeriesData" && opDef.contains("seriesIndex")
                   && opDef.contains("x") && opDef.contains("y")) {
            const auto seriesIndex = opDef["seriesIndex"].template get<int>();
            const auto x = opDef["x"].template get<double>();
            const auto y = opDef["y"].template get<double>();

            AppendSeriesData(seriesIndex, x, y);
        } else if (op == "setData" && opDef.contains("series") && opDef["series"].is_array()) {
            SetData(opDef["series"]);
        } else if (op == "setAxesDecimalDigits" && opDef.contains("x") && opDef.contains("y")) {
            const auto x = opDef["x"].template get<int>();
            const auto y = opDef["y"].template get<int>();

            SetAxesDecimalDigits(x, y);
        } else if (op == "setAxesAutoFit" && opDef.contains("enabled")) {
            const auto enabled = opDef["enabled"].template get<bool>();

            SetAxesAutoFit(enabled);
        } else if (op == "resetData") {
            ResetData();
        }
    }
};