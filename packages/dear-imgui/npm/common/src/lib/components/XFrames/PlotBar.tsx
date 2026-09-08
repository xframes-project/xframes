import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type PlotBarImperativeHandle = {
    setData: (data: { x: number; y: number }[], tickLabels?: string[]) => void;
    appendData: (x: number, y: number) => void;
    setSeriesData: (seriesData: { data: { x: number; y: number }[]; tickLabels?: string[] }[]) => void;
    appendSeriesData: (seriesIndex: number, x: number, y: number) => void;
    setAxesAutoFit: (enabled: boolean) => void;
    resetData: () => void;
};

export const PlotBar = forwardRef<PlotBarImperativeHandle, WidgetPropsMap["PlotBar"]>(
    (
        {
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
        }: WidgetPropsMap["PlotBar"],
        ref,
    ) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useWidgetRegistration(widgetRegistratonService, "table");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    setData: (data: { x: number; y: number }[], tickLabels?: string[]) => {
                        widgetRegistratonService.setPlotBarData(target, data, tickLabels);
                    },
                    appendData: (x: number, y: number) => {
                        widgetRegistratonService.appendDataToPlotLine(target, x, y);
                    },
                    setSeriesData: (seriesData: { data: { x: number; y: number }[]; tickLabels?: string[] }[]) => {
                        widgetRegistratonService.setPlotBarSeriesData(target, seriesData);
                    },
                    appendSeriesData: (seriesIndex: number, x: number, y: number) => {
                        widgetRegistratonService.appendPlotBarSeriesData(target, seriesIndex, x, y);
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
            <plot-bar
                id={idRef.current}
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
