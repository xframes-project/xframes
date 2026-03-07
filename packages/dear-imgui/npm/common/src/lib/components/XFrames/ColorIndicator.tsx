import { WidgetFunctionComponent, WidgetPropsMap } from "./types";

export const ColorIndicator: WidgetFunctionComponent<WidgetPropsMap["ColorIndicator"]> = ({
    color,
    shape,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <color-indicator
            color={color}
            shape={shape}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
