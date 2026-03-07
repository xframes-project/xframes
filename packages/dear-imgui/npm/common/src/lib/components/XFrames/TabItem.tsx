import { useRef } from "react";
import { PropsWithChildren, WidgetFunctionComponent, WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export const TabItem: WidgetFunctionComponent<PropsWithChildren & WidgetPropsMap["TabItem"]> = ({
    children,
    closeable,
    onChange,
    label,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    const widgetRegistratonService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistratonService.generateId());

    return (
        <tab-item
            id={idRef.current}
            label={label}
            closeable={closeable}
            onChange={onChange}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {children}
        </tab-item>
    );
};
