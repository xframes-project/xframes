import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type PlotLineImperativeHandle = {
    appendData: (x: number, y: number) => void;
    appendSeriesData: (seriesIndex: number, x: number, y: number) => void;
    setData: (seriesData: { data: { x: number; y: number }[] }[]) => void;
    setAxesDecimalDigits: (x: number, y: number) => void;
    setAxesAutoFit: (enabled: boolean) => void;
    resetData: () => void;
};

export const PlotLine = forwardRef<PlotLineImperativeHandle, WidgetPropsMap["PlotLine"]>(
    (
        {
            xAxisDecimalDigits,
            yAxisDecimalDigits,
            markerStyle,
            xAxisScale,
            yAxisScale,
            axisAutoFit,
            dataPointsLimit,
            xAxisLabel,
            yAxisLabel,
            showLegend,
            legendLocation,
            legendLabel,
            series,
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["PlotLine"],
        ref,
    ) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useWidgetRegistration(widgetRegistratonService, "table");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    appendData: (x: number, y: number) => {
                        widgetRegistratonService.appendDataToPlotLine(target, x, y);
                    },
                    appendSeriesData: (seriesIndex: number, x: number, y: number) => {
                        widgetRegistratonService.appendSeriesDataToPlotLine(
                            target,
                            seriesIndex,
                            x,
                            y,
                        );
                    },
                    setData: (seriesData: { data: { x: number; y: number }[] }[]) => {
                        widgetRegistratonService.setPlotLineData(target, seriesData);
                    },
                    setAxesDecimalDigits: (x: number, y: number) => {
                        widgetRegistratonService.setPlotLineAxesDecimalDigits(target, x, y);
                    },
                    setAxesAutoFit: (enabled: boolean) => {
                        widgetRegistratonService.setPlotLineAutoAxisFitEnabled(
                            target,
                            enabled,
                        );
                    },
                    resetData: () => {
                        widgetRegistratonService.resetPlotData(target);
                    },
                };
            },
            [],
        );

        return (
            <plot-line
                id={idRef.current}
                markerStyle={markerStyle}
                xAxisDecimalDigits={xAxisDecimalDigits}
                yAxisDecimalDigits={yAxisDecimalDigits}
                xAxisScale={xAxisScale}
                yAxisScale={yAxisScale}
                axisAutoFit={axisAutoFit}
                dataPointsLimit={dataPointsLimit}
                xAxisLabel={xAxisLabel}
                yAxisLabel={yAxisLabel}
                showLegend={showLegend}
                legendLocation={legendLocation}
                legendLabel={legendLabel}
                series={series}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
