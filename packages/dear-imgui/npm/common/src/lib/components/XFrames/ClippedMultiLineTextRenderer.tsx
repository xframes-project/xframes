import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type ClippedMultiLineTextRendererImperativeHandle = {
    appendTextToClippedMultiLineTextRenderer: (data: string) => void;
};

export const ClippedMultiLineTextRenderer = forwardRef<
    ClippedMultiLineTextRendererImperativeHandle,
    WidgetPropsMap["ClippedMultiLineTextRenderer"]
>(
    (
        {
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["ClippedMultiLineTextRenderer"],
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
                    appendTextToClippedMultiLineTextRenderer(data: string) {
                        widgetRegistratonService.appendTextToClippedMultiLineTextRenderer(
                            idRef.current,
                            data,
                        );
                    },
                };
            },
            [],
        );

        return (
            <clipped-multi-line-text-renderer
                id={idRef.current}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
