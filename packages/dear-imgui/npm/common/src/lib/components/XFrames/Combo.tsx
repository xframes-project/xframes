import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type ComboImperativeHandle = {
    setSelectedIndex: (index: number) => void;
};

export const Combo = forwardRef<ComboImperativeHandle, WidgetPropsMap["Combo"]>(
    (
        {
            placeholder,
            options,
            onChange,
            initialSelectedIndex,
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        },
        ref,
    ) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useWidgetRegistration(widgetRegistratonService, "widget");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    setSelectedIndex(index: number) {
                        widgetRegistratonService.setComboSelectedIndex(target, index);
                    },
                };
            },
            [],
        );

        return (
            <combo
                placeholder={placeholder}
                id={idRef.current}
                initialSelectedIndex={initialSelectedIndex}
                options={options}
                onChange={onChange}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
