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

    if (ImPlot::BeginPlot("plot_scatter", size, ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend | ImPlotFlags_NoTitle)) {
        if (m_axisAutoFit) {
            ImPlot::SetupAxes("x","y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        } else {
            ImPlot::SetupAxes("x","y");
        }

        double* x_valuesPtr = m_xValues.data();
        double* y_valuesPtr = m_yValues.data();

        int count = static_cast<int>(m_xValues.size());

        ImPlot::PlotScatter("scatter-plot", x_valuesPtr, y_valuesPtr, count);

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
