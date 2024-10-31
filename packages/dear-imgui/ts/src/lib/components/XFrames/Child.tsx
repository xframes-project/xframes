import { PropsWithChildren, WidgetFunctionComponent, WidgetPropsMap } from "./types";

// todo: perhaps we can come up with a way to deal with 'real' fragments
export const Child: WidgetFunctionComponent<PropsWithChildren & WidgetPropsMap["Child"]> = ({
    children,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <child
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {children}
        </child>
    );
};
