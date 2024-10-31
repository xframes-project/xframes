import { PropsWithChildren, WidgetFunctionComponent, WidgetPropsMap } from "./types";

// todo: perhaps we can come up with a way to deal with 'real' fragments
export const DIWindow: WidgetFunctionComponent<PropsWithChildren & WidgetPropsMap["DIWindow"]> = ({
    title,
    width,
    height,
    children,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <di-window
            width={width}
            height={height}
            title={title}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {children}
        </di-window>
    );
};
