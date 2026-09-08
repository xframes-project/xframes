import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type PlotScatterImperativeHandle = {
    setData: (data: { x: number; y: number }[]) => void;
    appendData: (x: number, y: number) => void;
    setAxesAutoFit: (enabled: boolean) => void;
    resetData: () => void;
};

export const PlotScatter = forwardRef<PlotScatterImperativeHandle, WidgetPropsMap["PlotScatter"]>(
    (
        {
            axisAutoFit,
            dataPointsLimit,
            xAxisLabel,
            yAxisLabel,
            showLegend,
            legendLocation,
            legendLabel,
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["PlotScatter"],
        ref,
    ) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useWidgetRegistration(widgetRegistratonService, "table");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    setData: (data: { x: number; y: number }[]) => {
                        widgetRegistratonService.setPlotScatterData(target, data);
                    },
                    appendData: (x: number, y: number) => {
                        widgetRegistratonService.appendDataToPlotLine(target, x, y);
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
            <plot-scatter
                id={idRef.current}
                axisAutoFit={axisAutoFit}
                dataPointsLimit={dataPointsLimit}
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
