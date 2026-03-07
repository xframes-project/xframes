#include <imgui.h>

#include "widget/plot_heatmap.h"
#include "xframes.h"

bool PlotHeatmap::HasCustomWidth() {
    return false;
}

bool PlotHeatmap::HasCustomHeight() {
    return false;
}

void PlotHeatmap::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    auto size = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    ImPlot::PushColormap(m_colormap);

    if (ImPlot::BeginPlot("plot_heatmap", size, ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend | ImPlotFlags_NoTitle)) {
        ImPlotAxisFlags axFlags = ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks;
        if (m_axisAutoFit) {
            axFlags |= ImPlotAxisFlags_AutoFit;
        } else {
            axFlags |= ImPlotAxisFlags_Lock;
        }
        const char* xLabel = m_xAxisLabel.empty() ? nullptr : m_xAxisLabel.c_str();
        const char* yLabel = m_yAxisLabel.empty() ? nullptr : m_yAxisLabel.c_str();
        ImPlot::SetupAxes(xLabel, yLabel, axFlags, axFlags);

        if (m_rows > 0 && m_cols > 0 && !m_values.empty()) {
            ImPlot::PlotHeatmap("heatmap", m_values.data(), m_rows, m_cols,
                m_scaleMin, m_scaleMax, nullptr,
                ImPlotPoint(0, 0), ImPlotPoint(static_cast<double>(m_cols), static_cast<double>(m_rows)));
        }

        ImPlot::EndPlot();
    }

    ImPlot::PopColormap();

    ImGui::PopID();
};

void PlotHeatmap::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("axisAutoFit")) {
        const auto axisAutoFit = widgetPatchDef["axisAutoFit"].template get<bool>();
        SetAxesAutoFit(axisAutoFit);
    }

    if (widgetPatchDef.contains("scaleMin")) {
        m_scaleMin = widgetPatchDef["scaleMin"].template get<double>();
    }

    if (widgetPatchDef.contains("scaleMax")) {
        m_scaleMax = widgetPatchDef["scaleMax"].template get<double>();
    }

    if (widgetPatchDef.contains("colormap")) {
        m_colormap = widgetPatchDef["colormap"].template get<int>();
    }

    if (widgetPatchDef.contains("xAxisLabel") && widgetPatchDef["xAxisLabel"].is_string()) {
        m_xAxisLabel = widgetPatchDef["xAxisLabel"].template get<std::string>();
    }
    if (widgetPatchDef.contains("yAxisLabel") && widgetPatchDef["yAxisLabel"].is_string()) {
        m_yAxisLabel = widgetPatchDef["yAxisLabel"].template get<std::string>();
    }
};

bool PlotHeatmap::HasInternalOps() {
    return true;
}

void PlotHeatmap::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        const auto op = opDef["op"].template get<std::string>();

        if (op == "setData" && opDef.contains("rows") && opDef.contains("cols") && opDef.contains("values") && opDef["values"].is_array()) {
            const auto rows = opDef["rows"].template get<int>();
            const auto cols = opDef["cols"].template get<int>();

            std::vector<double> values;
            values.reserve(rows * cols);

            for (auto& val : opDef["values"]) {
                values.push_back(val.template get<double>());
            }

            SetData(rows, cols, values);
        } else if (op == "setAxesAutoFit" && opDef.contains("enabled")) {
            const auto enabled = opDef["enabled"].template get<bool>();
            SetAxesAutoFit(enabled);
        } else if (op == "resetData") {
            ResetData();
        }
    }
};
