import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type ImageImperativeHandle = {
    reload: () => void;
};

export const Image = forwardRef<ImageImperativeHandle, WidgetPropsMap["Image"]>(
    (
        {
            url,
            width,
            height,
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["Image"],
        ref,
    ) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useWidgetRegistration(widgetRegistratonService, "map");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    reload() {
                        widgetRegistratonService.reloadImage(target);
                    },
                };
            },
            [],
        );

        return (
            <di-image
                id={idRef.current}
                url={url}
                width={width}
                height={height}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
