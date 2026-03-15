import { forwardRef, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type JanetCanvasImperativeHandle = {
    setScript: (script: string) => void;
    setScriptFile: (path: string) => void;
    setData: (data: any) => void;
    clear: () => void;
    loadTexture: (textureId: string, source: string) => void;
    unloadTexture: (textureId: string) => void;
    reloadTexture: (textureId: string, source: string) => void;
};

export const JanetCanvas = forwardRef<JanetCanvasImperativeHandle, WidgetPropsMap["JanetCanvas"]>(
    ({ style, hoverStyle, activeStyle, disabledStyle, onScriptError }: WidgetPropsMap["JanetCanvas"], ref) => {
        const widgetRegistrationService = useWidgetRegistrationService();
        const idRef = useRef(widgetRegistrationService.generateId());

        useImperativeHandle(
            ref,
            () => {
                return {
                    setScript(script: string) {
                        widgetRegistrationService.setCanvasScript(idRef.current, script);
                    },
                    setScriptFile(path: string) {
                        widgetRegistrationService.setCanvasScriptFile(idRef.current, path);
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
                    reloadTexture(textureId: string, source: string) {
                        widgetRegistrationService.reloadCanvasTexture(idRef.current, textureId, source);
                    },
                };
            },
            [],
        );

        return (
            <di-janet-canvas
                id={idRef.current}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
                onScriptError={onScriptError}
            />
        );
    },
);
