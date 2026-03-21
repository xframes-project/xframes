#include <imgui.h>

#include "widget/plot_bar.h"
#include "xframes.h"

bool PlotBar::HasCustomWidth() {
    return false;
}

bool PlotBar::HasCustomHeight() {
    return false;
}

void PlotBar::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    auto size = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    ImPlotFlags plotFlags = ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoTitle;
    if (!m_showLegend) plotFlags |= ImPlotFlags_NoLegend;

    if (ImPlot::BeginPlot("plot_bar", size, plotFlags)) {
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

        if (!m_tickLabelPtrs.empty()) {
            // Use xValues from first non-empty series for tick positions
            for (const auto& series : m_series) {
                if (!series.xValues.empty() && m_tickLabelPtrs.size() == series.xValues.size()) {
                    ImPlot::SetupAxisTicks(ImAxis_X1, series.xValues.data(),
                        static_cast<int>(m_tickLabelPtrs.size()), m_tickLabelPtrs.data());
                    break;
                }
            }
        }

        double barSize = 0.67;

        for (const auto& series : m_series) {
            if (series.xValues.empty()) continue;
            ImPlot::PlotBars(
                series.label.c_str(),
                series.xValues.data(),
                series.yValues.data(),
                static_cast<int>(series.xValues.size()),
                barSize
            );
        }

        ImPlot::EndPlot();
    }

    ImGui::PopID();
};

void PlotBar::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

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

    if (widgetPatchDef.contains("series") && widgetPatchDef["series"].is_array()) {
        const auto& seriesDef = widgetPatchDef["series"];
        size_t newCount = seriesDef.size();

        if (newCount > m_series.size()) {
            for (size_t i = m_series.size(); i < newCount; i++) {
                PlotBarSeries s;
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
        }
    }

    // Sync single-series backward compat
    if (!widgetPatchDef.contains("series") && m_series.size() == 1) {
        m_series[0].label = m_legendLabel;
    }
};

bool PlotBar::HasInternalOps() {
    return true;
}

void PlotBar::HandleInternalOp(const json& opDef) {
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
        } else if (op == "setSeriesData" && opDef.contains("series") && opDef["series"].is_array()) {
            SetSeriesData(opDef["series"]);
        } else if (op == "setData" && opDef.contains("data") && opDef["data"].is_array()) {
            std::vector<double> xs;
            std::vector<double> ys;

            for (auto& [itemKey, item] : opDef["data"].items()) {
                if (item.is_object()) {
                    xs.push_back(item["x"].template get<double>());
                    ys.push_back(item["y"].template get<double>());
                }
            }

            if (opDef.contains("tickLabels") && opDef["tickLabels"].is_array()) {
                std::vector<std::string> labels;
                for (auto& lbl : opDef["tickLabels"]) {
                    labels.push_back(lbl.template get<std::string>());
                }
                SetData(xs, ys, labels);
            } else {
                SetData(xs, ys);
            }
        } else if (op == "setAxesAutoFit" && opDef.contains("enabled")) {
            const auto enabled = opDef["enabled"].template get<bool>();

            SetAxesAutoFit(enabled);
        } else if (op == "resetData") {
            ResetData();
        }
    }
};
