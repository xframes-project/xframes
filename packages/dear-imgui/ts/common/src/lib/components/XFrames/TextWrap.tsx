import { PropsWithChildren, WidgetFunctionComponent, WidgetPropsMap } from "./types";

export const TextWrap: WidgetFunctionComponent<PropsWithChildren & WidgetPropsMap["TextWrap"]> = ({
    children,
    width,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <text-wrap
            width={width}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {children}
        </text-wrap>
    );
};
