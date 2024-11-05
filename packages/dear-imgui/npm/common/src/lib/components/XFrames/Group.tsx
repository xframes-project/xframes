import { PropsWithChildren, WidgetFunctionComponent, WidgetPropsMap } from "./types";

// todo: perhaps we can come up with a way to deal with 'real' fragments
export const Group: WidgetFunctionComponent<PropsWithChildren & WidgetPropsMap["Group"]> = ({
    children,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <group
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {children}
        </group>
    );
};
