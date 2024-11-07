import { useRef } from "react";
import { WidgetFunctionComponent, WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export const Button: WidgetFunctionComponent<WidgetPropsMap["Button"]> = ({
    label,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
    onClick,
}) => {
    const widgetRegistratonService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistratonService.generateId());

    return (
        <di-button
            label={label}
            id={idRef.current}
            onClick={onClick}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
