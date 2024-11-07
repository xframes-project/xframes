import { useRef } from "react";
import { PropsWithChildren, WidgetFunctionComponent, WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export const CollapsingHeader: WidgetFunctionComponent<
    PropsWithChildren & WidgetPropsMap["CollapsingHeader"]
> = ({ children, label, style, hoverStyle, activeStyle, disabledStyle }) => {
    const widgetRegistratonService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistratonService.generateId());

    return (
        <collapsing-header
            id={idRef.current}
            label={label}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {children}
        </collapsing-header>
    );
};
