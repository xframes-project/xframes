import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type MapImperativeHandle = {
    render: (centerX: number, centerY: number, zoom: number) => void;
};

export const MapView = forwardRef<MapImperativeHandle, WidgetPropsMap["MapView"]>(
    ({ style, hoverStyle, activeStyle, disabledStyle }: WidgetPropsMap["MapView"], ref) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useRef(widgetRegistratonService.generateId());

        useEffect(() => {
            widgetRegistratonService.registerMap(idRef.current);
        }, [widgetRegistratonService]);

        useImperativeHandle(
            ref,
            () => {
                return {
                    render(centerX: number, centerY: number, zoom: number) {
                        widgetRegistratonService.renderMap(idRef.current, centerX, centerY, zoom);
                    },
                };
            },
            [],
        );

        return (
            <map-view
                id={idRef.current}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
