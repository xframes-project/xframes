import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type PlotPieChartImperativeHandle = {
    setData: (data: { label: string; value: number }[]) => void;
    resetData: () => void;
};

export const PlotPieChart = forwardRef<PlotPieChartImperativeHandle, WidgetPropsMap["PlotPieChart"]>(
    (
        {
            labelFormat,
            angle0,
            normalize,
            showLegend,
            legendLocation,
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["PlotPieChart"],
        ref,
    ) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useWidgetRegistration(widgetRegistratonService, "table");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    setData: (data: { label: string; value: number }[]) => {
                        widgetRegistratonService.setPlotPieChartData(target, data);
                    },
                    resetData: () => {
                        widgetRegistratonService.resetPlotData(target);
                    },
                };
            },
            [],
        );

        return (
            <plot-pie-chart
                id={idRef.current}
                labelFormat={labelFormat}
                angle0={angle0}
                normalize={normalize}
                showLegend={showLegend}
                legendLocation={legendLocation}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
