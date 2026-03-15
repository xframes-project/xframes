import { forwardRef, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type CanvasImperativeHandle = {
    setScript: (script: string) => void;
    setData: (data: any) => void;
    clear: () => void;
};

export const Canvas = forwardRef<CanvasImperativeHandle, WidgetPropsMap["Canvas"]>(
    ({ style, hoverStyle, activeStyle, disabledStyle }: WidgetPropsMap["Canvas"], ref) => {
        const widgetRegistrationService = useWidgetRegistrationService();
        const idRef = useRef(widgetRegistrationService.generateId());

        useImperativeHandle(
            ref,
            () => {
                return {
                    setScript(script: string) {
                        widgetRegistrationService.setCanvasScript(idRef.current, script);
                    },
                    setData(data: any) {
                        widgetRegistrationService.setCanvasData(idRef.current, data);
                    },
                    clear() {
                        widgetRegistrationService.clearCanvas(idRef.current);
                    },
                };
            },
            [],
        );

        return (
            <di-canvas
                id={idRef.current}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
