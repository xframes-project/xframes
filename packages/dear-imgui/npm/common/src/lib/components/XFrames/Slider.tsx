import { useRef } from "react";
import { WidgetFunctionComponent, WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export const Slider: WidgetFunctionComponent<WidgetPropsMap["Slider"]> = ({
    min,
    max,
    onChange,
    defaultValue,
    sliderType = "default",
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    const widgetRegistratonService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistratonService.generateId());

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
};
