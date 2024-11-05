import { WidgetFunctionComponent, WidgetPropsMap } from "./types";

export const Separator: WidgetFunctionComponent<WidgetPropsMap["Separator"]> = ({
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <separator
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
