import { useRef } from "react";
import { useWidgetRegistrationService } from "../../hooks";
import { WidgetFunctionComponent, WidgetPropsMap } from "./types";

export const MultiSlider: WidgetFunctionComponent<WidgetPropsMap["MultiSlider"]> = ({
    min,
    max,
    onChange,
    defaultValues,
    numValues,
    decimalDigits,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    const widgetRegistratonService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistratonService.generateId());

    if (Array.isArray(defaultValues) && defaultValues.length !== numValues) {
        // todo: Mismatch! What to do?
    }

    return (
        <multi-slider
            id={idRef.current}
            defaultValues={defaultValues}
            min={min}
            max={max}
            decimalDigits={decimalDigits}
            numValues={numValues}
            onChange={onChange}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
