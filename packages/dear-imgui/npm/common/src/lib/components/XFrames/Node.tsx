import { FunctionComponent, useRef } from "react";
import { NodeProps, PropsWithChildren } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export const Node: FunctionComponent<PropsWithChildren & NodeProps> = ({
    root,
    children,
    cull,
    trackMouseClickEvents,
    onClick,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    const widgetRegistratonService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistratonService.generateId());

    return (
        <node
            root={root}
            onClick={onClick}
            id={trackMouseClickEvents ? idRef.current : undefined}
            cull={cull}
            trackMouseClickEvents={trackMouseClickEvents}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {children}
        </node>
    );
};
