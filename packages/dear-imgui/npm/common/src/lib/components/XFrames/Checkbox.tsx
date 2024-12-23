import { useRef } from "react";
import { WidgetFunctionComponent, WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export const Checkbox: WidgetFunctionComponent<WidgetPropsMap["Checkbox"]> = ({
    label,
    onChange,
    defaultChecked,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    const widgetRegistratonService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistratonService.generateId());

    return (
        <checkbox
            label={label}
            id={idRef.current}
            defaultChecked={defaultChecked}
            onChange={onChange}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
