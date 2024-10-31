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

    if (ImPlot::BeginPlot("plot_line", size, ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend | ImPlotFlags_NoTitle)) {
        if (m_axisAutoFit) {
            ImPlot::SetupAxes("x","y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        } else {
            ImPlot::SetupAxes("x","y");
            // TODO: compute axes limits?
            // ImPlot::SetupAxesLimits(0,15000,0,1000);
        }

        ImPlot::SetupAxisScale(ImAxis_X1, m_xAxisScale);
        ImPlot::SetupAxisScale(ImAxis_Y1, m_yAxisScale);

        if (m_xAxisDecimalDigits > 0) {
            ImPlot::SetupAxisFormat(ImAxis_X1, axisValueFormatter, (void*)m_xAxisDecimalDigits);
        }

        if (m_yAxisDecimalDigits > 0) {
            ImPlot::SetupAxisFormat(ImAxis_Y1, axisValueFormatter, (void*)m_yAxisDecimalDigits);
        }

        double* x_valuesPtr = m_xValues.data();
        double* y_valuesPtr = m_yValues.data();

        ImPlot::SetNextMarkerStyle(m_markerStyle);
        ImPlot::PlotLine("line-plot", x_valuesPtr, y_valuesPtr, m_xValues.size());

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