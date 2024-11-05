import { WidgetFunctionComponent, WidgetPropsMap } from "./types";

export const BulletText: WidgetFunctionComponent<WidgetPropsMap["BulletText"]> = ({
    text,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <bullet-text
            text={text}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
