#include <imgui.h>

#include "widget/plot_scatter.h"
#include "xframes.h"

bool PlotScatter::HasCustomWidth() {
    return false;
}

bool PlotScatter::HasCustomHeight() {
    return false;
}

void PlotScatter::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    auto size = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    ImPlotFlags plotFlags = ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoTitle;
    if (!m_showLegend) plotFlags |= ImPlotFlags_NoLegend;

    if (ImPlot::BeginPlot("plot_scatter", size, plotFlags)) {
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

        double* x_valuesPtr = m_xValues.data();
        double* y_valuesPtr = m_yValues.data();

        int count = static_cast<int>(m_xValues.size());

        ImPlot::PlotScatter(m_legendLabel.c_str(), x_valuesPtr, y_valuesPtr, count);

        ImPlot::EndPlot();
    }

    ImGui::PopID();
};

void PlotScatter::Patch(const json& widgetPatchDef, XFrames* view) {
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
};

bool PlotScatter::HasInternalOps() {
    return true;
}

void PlotScatter::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        const auto op = opDef["op"].template get<std::string>();

        if (op == "appendData" && opDef.contains("x") && opDef.contains("y")) {
            const auto x = opDef["x"].template get<double>();
            const auto y = opDef["y"].template get<double>();

            AppendData(x, y);
        } else if (op == "setData" && opDef.contains("data") && opDef["data"].is_array()) {
            std::vector<double> xs;
            std::vector<double> ys;

            for (auto& [itemKey, item] : opDef["data"].items()) {
                if (item.is_object()) {
                    xs.push_back(item["x"].template get<double>());
                    ys.push_back(item["y"].template get<double>());
                }
            }

            SetData(xs, ys);
        } else if (op == "setAxesAutoFit" && opDef.contains("enabled")) {
            const auto enabled = opDef["enabled"].template get<bool>();

            SetAxesAutoFit(enabled);
        } else if (op == "resetData") {
            ResetData();
        }
    }
};
