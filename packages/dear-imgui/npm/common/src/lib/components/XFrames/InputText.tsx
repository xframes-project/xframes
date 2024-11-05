import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { useWidgetRegistrationService } from "../../hooks";
import { WidgetPropsMap } from "./types";

export type InputTextImperativeHandle = {
    setValue: (value: string) => void;
};

export const InputText = forwardRef<InputTextImperativeHandle, WidgetPropsMap["InputText"]>(
    ({ onChange, defaultValue, hint, style, hoverStyle, activeStyle, disabledStyle }, ref) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useRef(widgetRegistratonService.generateId());

        useImperativeHandle(
            ref,
            () => {
                return {
                    setValue(value: string) {
                        widgetRegistratonService.setInputTextValue(idRef.current, value);
                    },
                };
            },
            [],
        );

        return (
            <input-text
                id={idRef.current}
                defaultValue={defaultValue}
                hint={hint}
                onChange={onChange}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
