import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type SliderImperativeHandle = {
    setValue: (value: number) => void;
};

export const Slider = forwardRef<SliderImperativeHandle, WidgetPropsMap["Slider"]>(
    ({
        min,
        max,
        onChange,
        defaultValue,
        sliderType = "default",
        style,
        hoverStyle,
        activeStyle,
        disabledStyle,
    }, ref) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useWidgetRegistration(widgetRegistratonService, "widget");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    setValue(value: number) {
                        widgetRegistratonService.setSliderValue(target, value);
                    },
                };
            },
            [],
        );

        return (
            <slider
                id={idRef.current}
                defaultValue={defaultValue}
                min={min}
                max={max}
                sliderType={sliderType}
                onChange={onChange}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);
