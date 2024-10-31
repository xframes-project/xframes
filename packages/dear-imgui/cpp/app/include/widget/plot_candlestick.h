#include <cstdint>

#include "shared.h"
#include "styled_widget.h"

using PlotCandlestickDates = std::vector<double>;
using PlotCandlestickOpens = std::vector<double>;
using PlotCandlestickCloses = std::vector<double>;
using PlotCandlestickLows = std::vector<double>;
using PlotCandlestickHighs = std::vector<double>;

using PlotCandlestickData = std::tuple<
    PlotCandlestickDates,
    PlotCandlestickOpens,
    PlotCandlestickCloses,
    PlotCandlestickLows,
    PlotCandlestickHighs
>;

class PlotCandlestick final : public StyledWidget {
private:
    PlotCandlestickDates m_dates;
    PlotCandlestickOpens m_opens;
    PlotCandlestickCloses m_closes;
    PlotCandlestickLows m_lows;
    PlotCandlestickHighs m_highs;

    int m_dataPointsLimit = 6000;

    bool m_axisAutoFit;

    bool m_showTooltip = true;

    // std::optional<ImVec4> bullCol;
    // std::optional<ImVec4> bearCol;

    ImVec4 m_bullCol = ImVec4(0.000f, 1.000f, 0.441f, 1.000f);
    ImVec4 m_bearCol = ImVec4(0.853f, 0.050f, 0.310f, 1.000f);

    float m_widthPercent = 0.25f;



public:
    static std::unique_ptr<PlotCandlestick> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        bool axisAutoFit = false;
        ImVec4 bullCol = ImVec4(0.000f, 1.000f, 0.441f, 1.000f);
        ImVec4 bearCol = ImVec4(0.853f, 0.050f, 0.310f, 1.000f);

        if (widgetDef.contains("axisAutoFit")) {
            axisAutoFit = widgetDef["axisAutoFit"].template get<bool>();
        }

        return std::make_unique<PlotCandlestick>(view, id, bullCol, bearCol, axisAutoFit, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
    }

    static int axisValueFormatter(double value, char* buff, int size, void* decimalPlaces) {
        if (value == 0) {
            return snprintf(buff, size,"0");
        }

        return snprintf(buff, size, "%.*f", reinterpret_cast<intptr_t>(decimalPlaces), value);
    };

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    PlotCandlestick(XFrames* view, const int id, const ImVec4& bullCol, const ImVec4& bearCol, const bool axisAutoFit, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
        m_type = "plot-candlestick";
        m_axisAutoFit = axisAutoFit;

        m_bullCol = bullCol;
        m_bearCol = bearCol;

        m_dates.reserve(m_dataPointsLimit);
        m_opens.reserve(m_dataPointsLimit);
        m_closes.reserve(m_dataPointsLimit);
        m_lows.reserve(m_dataPointsLimit);
        m_highs.reserve(m_dataPointsLimit);
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;

    void SetAxesAutoFit(const bool enabled) {
        m_axisAutoFit = enabled;
    }

    void SetData(PlotCandlestickData& data) {
        ResetData();

        auto& [dates, opens, closes, lows, highs] = data;

        m_dates.insert(m_dates.end(), dates.begin(), dates.end());
        m_opens.insert(m_opens.end(), opens.begin(), opens.end());
        m_closes.insert(m_closes.end(), closes.begin(), closes.end());
        m_lows.insert(m_lows.end(), lows.begin(), lows.end());
        m_highs.insert(m_highs.end(), highs.begin(), highs.end());
    }

    void ResetData() {
        m_dates.clear();
        m_opens.clear();
        m_closes.clear();
        m_lows.clear();
        m_highs.clear();
    }

    void RenderPlotCandlestick();
};
