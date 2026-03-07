import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type PlotHeatmapImperativeHandle = {
    setData: (rows: number, cols: number, values: number[]) => void;
    setAxesAutoFit: (enabled: boolean) => void;
    resetData: () => void;
};

export const PlotHeatmap = forwardRef<PlotHeatmapImperativeHandle, WidgetPropsMap["PlotHeatmap"]>(
    (
        {
            axisAutoFit,
            scaleMin,
            scaleMax,
            colormap,
            xAxisLabel,
            yAxisLabel,
            showLegend,
            legendLocation,
            legendLabel,
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["PlotHeatmap"],
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
                    setData: (rows: number, cols: number, values: number[]) => {
                        widgetRegistratonService.setPlotHeatmapData(
                            idRef.current,
                            rows,
                            cols,
                            values,
                        );
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
            <plot-heatmap
                id={idRef.current}
                axisAutoFit={axisAutoFit}
                scaleMin={scaleMin}
                scaleMax={scaleMax}
                colormap={colormap}
                xAxisLabel={xAxisLabel}
                yAxisLabel={yAxisLabel}
                showLegend={showLegend}
                legendLocation={legendLocation}
                legendLabel={legendLabel}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
