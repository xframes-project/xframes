import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
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
        const idRef = useRef(widgetRegistratonService.generateId());

        useEffect(() => {
            widgetRegistratonService.registerTable(idRef.current);
        }, [widgetRegistratonService]);

        useImperativeHandle(
            ref,
            () => {
                return {
                    setData: (data: { label: string; value: number }[]) => {
                        widgetRegistratonService.setPlotPieChartData(idRef.current, data);
                    },
                    resetData: () => {
                        widgetRegistratonService.resetPlotData(idRef.current);
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
