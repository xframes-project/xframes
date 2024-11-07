import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { PlotCandlestickDataItem, WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type PlotCandlestickImperativeHandle = {
    setData: (data: PlotCandlestickDataItem[]) => void;
    setAxesAutoFit: (enabled: boolean) => void;
    resetData: () => void;
};

export const PlotCandlestick = forwardRef<
    PlotCandlestickImperativeHandle,
    WidgetPropsMap["PlotCandlestick"]
>(
    (
        {
            axisAutoFit,
            bearColor,
            bullColor,
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["PlotCandlestick"],
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
                    setData: (data: PlotCandlestickDataItem[]) => {
                        widgetRegistratonService.setPlotCandlestickData(idRef.current, data);
                    },
                    setAxesAutoFit: (enabled: boolean) => {
                        widgetRegistratonService.setPlotCandlestickAutoAxisFitEnabled(
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
            <plot-candlestick
                id={idRef.current}
                bearColor={bearColor}
                bullColor={bullColor}
                axisAutoFit={axisAutoFit}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
