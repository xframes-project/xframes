#include <imgui.h>

#include "widget/plot_pie_chart.h"
#include "xframes.h"

bool PlotPieChart::HasCustomWidth() {
    return false;
}

bool PlotPieChart::HasCustomHeight() {
    return false;
}

void PlotPieChart::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    auto size = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    ImPlotFlags plotFlags = ImPlotFlags_Equal | ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoTitle;
    if (!m_showLegend) plotFlags |= ImPlotFlags_NoLegend;

    if (ImPlot::BeginPlot("plot_pie_chart", size, plotFlags)) {
        if (m_showLegend) {
            ImPlot::SetupLegend(static_cast<ImPlotLocation>(m_legendLocation));
        }
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, 1, 0, 1);

        if (!m_values.empty()) {
            ImPlotPieChartFlags pieFlags = 0;
            if (m_normalize) pieFlags |= ImPlotPieChartFlags_Normalize;

            ImPlot::PlotPieChart(m_labelPtrs.data(), m_values.data(),
                static_cast<int>(m_values.size()),
                0.5, 0.5, 0.4, m_labelFormat.c_str(), m_angle0, pieFlags);
        }

        ImPlot::EndPlot();
    }

    ImGui::PopID();
};

void PlotPieChart::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("labelFormat") && widgetPatchDef["labelFormat"].is_string()) {
        m_labelFormat = widgetPatchDef["labelFormat"].template get<std::string>();
    }
    if (widgetPatchDef.contains("angle0")) {
        m_angle0 = widgetPatchDef["angle0"].template get<double>();
    }
    if (widgetPatchDef.contains("normalize")) {
        m_normalize = widgetPatchDef["normalize"].template get<bool>();
    }
    if (widgetPatchDef.contains("showLegend")) {
        m_showLegend = widgetPatchDef["showLegend"].template get<bool>();
    }
    if (widgetPatchDef.contains("legendLocation")) {
        m_legendLocation = widgetPatchDef["legendLocation"].template get<int>();
    }
};

bool PlotPieChart::HasInternalOps() {
    return true;
}

void PlotPieChart::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        const auto op = opDef["op"].template get<std::string>();

        if (op == "setData" && opDef.contains("data") && opDef["data"].is_array()) {
            std::vector<std::string> labels;
            std::vector<double> values;

            for (auto& [itemKey, item] : opDef["data"].items()) {
                if (item.is_object()) {
                    labels.push_back(item["label"].template get<std::string>());
                    values.push_back(item["value"].template get<double>());
                }
            }

            SetData(labels, values);
        } else if (op == "resetData") {
            ResetData();
        }
    }
};
