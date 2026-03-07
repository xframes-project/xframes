import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
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
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["PlotScatter"],
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
                    setData: (data: { x: number; y: number }[]) => {
                        widgetRegistratonService.setPlotScatterData(idRef.current, data);
                    },
                    appendData: (x: number, y: number) => {
                        widgetRegistratonService.appendDataToPlotLine(idRef.current, x, y);
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
            <plot-scatter
                id={idRef.current}
                axisAutoFit={axisAutoFit}
                dataPointsLimit={dataPointsLimit}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
