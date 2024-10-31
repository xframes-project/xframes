#include <imgui.h>
#include <implot.h>
#include <implot_internal.h>
#include "widget/plot_candlestick.h"
#include "shared.h"
#include "xframes.h"

bool PlotCandlestick::HasCustomWidth() {
    return false;
}

bool PlotCandlestick::HasCustomHeight() {
    return false;
}

void PlotCandlestick::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);

    auto size = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

    // if (ImPlot::BeginPlot("plot", size, ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend | ImPlotFlags_NoTitle)) {
        // if (m_axisAutoFit) {
        //     ImPlot::SetupAxes("x","y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        // } else {
        //     ImPlot::SetupAxes("x","y");
        // }

        // double* x_valuesPtr = m_xValues.data();
        // double* y_valuesPtr = m_yValues.data();

        // RenderPlotCandlestick();

    if (ImPlot::BeginPlot("Candlestick Chart",ImVec2(-1,0))) {
        // ImPlot::SetupAxes(nullptr,nullptr,0,ImPlotAxisFlags_AutoFit|ImPlotAxisFlags_RangeFit);
        ImPlot::SetupAxes(nullptr,nullptr,0,ImPlotAxisFlags_AutoFit);
        // ImPlot::SetupAxesLimits(1546300800, 1571961600, 1250, 1600);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        // ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 1546300800, 1571961600);
        // ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 60*60*24*14, 1571961600-1546300800);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "$%.4f");
        RenderPlotCandlestick();
        ImPlot::EndPlot();
    }

        // ImPlot::EndPlot();
    // }

    ImGui::PopID();
};

// todo: move to shared.h (figure out why it won't link...)
template <typename T>
int BinarySearch(const std::vector<T>& arr, int l, int r, T x) {
    if (r >= l) {
        int mid = l + (r - l) / 2;
        if (arr[mid] == x)
            return mid;
        if (arr[mid] > x)
            return BinarySearch(arr, l, mid - 1, x);
        return BinarySearch(arr, mid + 1, r, x);
    }
    return -1;
}

// https://github.com/epezent/implot/issues/290
void PlotCandlestick::RenderPlotCandlestick() {
    // get ImGui window DrawList
    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    // calc real value width
    double half_width = m_dates.size() > 1 ? (m_dates[1] - m_dates[0]) * m_widthPercent : m_widthPercent;

    // custom tool
    if (ImPlot::IsPlotHovered() && m_showTooltip) {
        ImPlotPoint mouse   = ImPlot::GetPlotMousePos();
        mouse.x             = ImPlot::RoundTime(ImPlotTime::FromDouble(mouse.x), ImPlotTimeUnit_Day).ToDouble();
        float  tool_l       = ImPlot::PlotToPixels(mouse.x - half_width * 1.5, mouse.y).x;
        float  tool_r       = ImPlot::PlotToPixels(mouse.x + half_width * 1.5, mouse.y).x;
        float  tool_t       = ImPlot::GetPlotPos().y;
        float  tool_b       = tool_t + ImPlot::GetPlotSize().y;
        ImPlot::PushPlotClipRect();
        draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b), IM_COL32(128,128,128,64));
        ImPlot::PopPlotClipRect();
        // find mouse location index
        int idx = BinarySearch(m_dates, 0, (int)m_dates.size() - 1, mouse.x);
        // render tool tip (won't be affected by plot clip rect)
        if (idx != -1) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
            ImGui::BeginTooltip();
            char buff[32];
            ImPlot::FormatDate(ImPlotTime::FromDouble(m_dates[idx]),buff,32,ImPlotDateFmt_DayMoYr,ImPlot::GetStyle().UseISO8601);
            ImGui::Text("Day:   %s",  buff);
            ImGui::Text("Open:  $%.2f", m_opens[idx]);
            ImGui::Text("Close: $%.2f", m_closes[idx]);
            ImGui::Text("Low:   $%.2f", m_lows[idx]);
            ImGui::Text("High:  $%.2f", m_highs[idx]);
            ImGui::EndTooltip();
            ImGui::PopStyleColor();
        }
    }

    // begin plot item
    if (ImPlot::BeginItem("plot_candlestick")) {
        // override legend icon color
        ImPlot::GetCurrentItem()->Color = IM_COL32(64,64,64,255);
        // fit data if requested
        if (ImPlot::FitThisFrame()) {
            for (int i = 0; i < m_dates.size(); ++i) {
                ImPlot::FitPoint(ImPlotPoint(m_dates[i], m_lows[i]));
                ImPlot::FitPoint(ImPlotPoint(m_dates[i], m_highs[i]));
            }
        }
        // render data
        for (int i = 0; i < m_dates.size(); ++i) {
            ImVec2 open_pos  = ImPlot::PlotToPixels(m_dates[i] - half_width, m_opens[i]);
            ImVec2 close_pos = ImPlot::PlotToPixels(m_dates[i] + half_width, m_closes[i]);
            ImVec2 low_pos   = ImPlot::PlotToPixels(m_dates[i], m_lows[i]);
            ImVec2 high_pos  = ImPlot::PlotToPixels(m_dates[i], m_highs[i]);
            ImU32 color      = ImGui::GetColorU32(m_opens[i] > m_closes[i] ? m_bearCol : m_bullCol);
            draw_list->AddLine(low_pos, high_pos, color);
            draw_list->AddRectFilled(open_pos, close_pos, color);
        }

        // end plot item
        ImPlot::EndItem();
    }
};

void PlotCandlestick::Patch(const json& widgetPatchDef, ReactImgui* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("axisAutoFit")) {
        const auto axisAutoFit = widgetPatchDef["axisAutoFit"].template get<bool>();
        SetAxesAutoFit(axisAutoFit);
    }
};

bool PlotCandlestick::HasInternalOps() {
    return true;
}

// void ReactImgui::RenderMap(int id, double centerX, double centerY, int zoom)
void PlotCandlestick::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        const auto op = opDef["op"].template get<std::string>();

        if (op == "setData" && opDef.contains("data") && opDef["data"].is_array()) {
            PlotCandlestickDates dates;
            PlotCandlestickOpens opens;
            PlotCandlestickCloses closes;
            PlotCandlestickLows lows;
            PlotCandlestickHighs highs;

            for (auto& [itemKey, item] : opDef["data"].items()) {
                if (item.is_object()) {
                    dates.push_back(item["date"].template get<double>());
                    opens.push_back(item["open"].template get<double>());
                    closes.push_back(item["close"].template get<double>());
                    lows.push_back(item["low"].template get<double>());
                    highs.push_back(item["high"].template get<double>());
                }
            }

            auto data = std::make_tuple(dates, opens, closes, lows, highs);

            SetData(data);
        } else if (op == "setAxesAutoFit" && opDef.contains("enabled")) {
            const auto enabled = opDef["enabled"].template get<bool>();

            SetAxesAutoFit(enabled);
        } else if (op == "resetData") {
            ResetData();
        }
    }
};