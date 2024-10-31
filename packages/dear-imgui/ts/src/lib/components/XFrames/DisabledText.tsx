import { WidgetFunctionComponent, WidgetPropsMap } from "./types";

export const DisabledText: WidgetFunctionComponent<WidgetPropsMap["DisabledText"]> = ({
    text,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <disabled-text
            text={text}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
