import { WidgetFunctionComponent, WidgetPropsMap } from "./types";

export const UnformattedText: WidgetFunctionComponent<WidgetPropsMap["UnformattedText"]> = ({
    text,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <unformatted-text
            text={text}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
