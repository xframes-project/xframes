import { forwardRef, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type CanvasImperativeHandle = {
    setScript: (script: string) => void;
    setData: (data: any) => void;
    clear: () => void;
    loadTexture: (textureId: string, source: string) => void;
    unloadTexture: (textureId: string) => void;
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
                    loadTexture(textureId: string, source: string) {
                        widgetRegistrationService.loadCanvasTexture(idRef.current, textureId, source);
                    },
                    unloadTexture(textureId: string) {
                        widgetRegistrationService.unloadCanvasTexture(idRef.current, textureId);
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
