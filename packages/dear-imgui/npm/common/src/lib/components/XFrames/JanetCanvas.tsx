import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type JanetCanvasImperativeHandle = {
    /** Scripts animate by default. Disable for static content that can settle. */
    setContinuous: (continuous: boolean) => void;
    /** Request one redraw, including while continuous rendering is disabled. */
    redraw: () => void;
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
        const idRef = useWidgetRegistration(widgetRegistrationService, "widget");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistrationService.captureWidget(idRef.current);
                return {
                    setContinuous(continuous: boolean) {
                        widgetRegistrationService.setCanvasContinuous(target, continuous);
                    },
                    redraw() {
                        widgetRegistrationService.redrawCanvas(target);
                    },
                    setScript(script: string) {
                        widgetRegistrationService.setCanvasScript(target, script);
                    },
                    setScriptFile(path: string) {
                        widgetRegistrationService.setCanvasScriptFile(target, path);
                    },
                    setData(data: any) {
                        widgetRegistrationService.setCanvasData(target, data);
                    },
                    clear() {
                        widgetRegistrationService.clearCanvas(target);
                    },
                    loadTexture(textureId: string, source: string) {
                        widgetRegistrationService.loadCanvasTexture(target, textureId, source);
                    },
                    unloadTexture(textureId: string) {
                        widgetRegistrationService.unloadCanvasTexture(target, textureId);
                    },
                    reloadTexture(textureId: string, source: string) {
                        widgetRegistrationService.reloadCanvasTexture(target, textureId, source);
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
