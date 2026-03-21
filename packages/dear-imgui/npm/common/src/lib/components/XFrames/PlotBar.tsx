import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
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
        const idRef = useRef(widgetRegistratonService.generateId());

        useEffect(() => {
            widgetRegistratonService.registerTable(idRef.current);
        }, [widgetRegistratonService]);

        useImperativeHandle(
            ref,
            () => {
                return {
                    setData: (data: { x: number; y: number }[], tickLabels?: string[]) => {
                        widgetRegistratonService.setPlotBarData(idRef.current, data, tickLabels);
                    },
                    appendData: (x: number, y: number) => {
                        widgetRegistratonService.appendDataToPlotLine(idRef.current, x, y);
                    },
                    setSeriesData: (seriesData: { data: { x: number; y: number }[]; tickLabels?: string[] }[]) => {
                        widgetRegistratonService.setPlotBarSeriesData(idRef.current, seriesData);
                    },
                    appendSeriesData: (seriesIndex: number, x: number, y: number) => {
                        widgetRegistratonService.appendPlotBarSeriesData(idRef.current, seriesIndex, x, y);
                    },
                    setAxesAutoFit: (enabled: boolean) => {
                        widgetRegistratonService.setPlotLineAutoAxisFitEnabled(
                            idRef.current,
                            enabled,
                        );
                    },
                    resetData: () => {
                        widgetRegistratonService.resetPlotData(idRef.current);
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
