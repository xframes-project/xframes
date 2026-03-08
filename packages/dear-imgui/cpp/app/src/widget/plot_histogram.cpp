#include <imgui.h>

#include "widget/plot_histogram.h"
#include "xframes.h"

bool PlotHistogram::HasCustomWidth() {
    return false;
}

bool PlotHistogram::HasCustomHeight() {
    return false;
}

void PlotHistogram::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    auto size = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    ImPlotFlags plotFlags = ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoTitle;
    if (!m_showLegend) plotFlags |= ImPlotFlags_NoLegend;

    if (ImPlot::BeginPlot("plot_histogram", size, plotFlags)) {
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

        if (!m_values.empty()) {
            ImPlot::PlotHistogram(m_legendLabel.c_str(), m_values.data(),
                static_cast<int>(m_values.size()), m_bins);
        }

        ImPlot::EndPlot();
    }

    ImGui::PopID();
};

void PlotHistogram::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("axisAutoFit")) {
        const auto axisAutoFit = widgetPatchDef["axisAutoFit"].template get<bool>();
        SetAxesAutoFit(axisAutoFit);
    }

    if (widgetPatchDef.contains("bins")) {
        m_bins = widgetPatchDef["bins"].template get<int>();
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
};

bool PlotHistogram::HasInternalOps() {
    return true;
}

void PlotHistogram::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        const auto op = opDef["op"].template get<std::string>();

        if (op == "appendData" && opDef.contains("value")) {
            const auto value = opDef["value"].template get<double>();

            AppendData(value);
        } else if (op == "setData" && opDef.contains("data") && opDef["data"].is_array()) {
            std::vector<double> values;

            for (auto& [itemKey, item] : opDef["data"].items()) {
                if (item.is_number()) {
                    values.push_back(item.template get<double>());
                }
            }

            SetData(values);
        } else if (op == "setAxesAutoFit" && opDef.contains("enabled")) {
            const auto enabled = opDef["enabled"].template get<bool>();

            SetAxesAutoFit(enabled);
        } else if (op == "resetData") {
            ResetData();
        }
    }
};
