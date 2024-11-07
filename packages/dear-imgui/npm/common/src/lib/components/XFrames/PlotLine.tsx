import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type PlotLineImperativeHandle = {
    appendData: (x: number, y: number) => void;
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
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["PlotLine"],
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
                    appendData: (x: number, y: number) => {
                        widgetRegistratonService.appendDataToPlotLine(idRef.current, x, y);
                    },
                    setAxesDecimalDigits: (x: number, y: number) => {
                        widgetRegistratonService.setPlotLineAxesDecimalDigits(idRef.current, x, y);
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
            <plot-line
                id={idRef.current}
                markerStyle={markerStyle}
                xAxisDecimalDigits={xAxisDecimalDigits}
                yAxisDecimalDigits={yAxisDecimalDigits}
                xAxisScale={xAxisScale}
                yAxisScale={yAxisScale}
                axisAutoFit={axisAutoFit}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
