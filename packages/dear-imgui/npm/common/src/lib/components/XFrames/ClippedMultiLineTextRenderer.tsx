import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
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
        const idRef = useWidgetRegistration(widgetRegistratonService, "table");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    appendTextToClippedMultiLineTextRenderer(data: string) {
                        widgetRegistratonService.appendTextToClippedMultiLineTextRenderer(
                            target,
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
