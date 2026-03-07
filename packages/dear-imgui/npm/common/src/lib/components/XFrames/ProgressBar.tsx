import { WidgetFunctionComponent, WidgetPropsMap } from "./types";

export const ProgressBar: WidgetFunctionComponent<WidgetPropsMap["ProgressBar"]> = ({
    fraction,
    overlay,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <progress-bar
            fraction={fraction}
            overlay={overlay}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
