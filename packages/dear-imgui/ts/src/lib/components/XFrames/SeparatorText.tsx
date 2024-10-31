import { WidgetFunctionComponent, WidgetPropsMap } from "./types";

export type SeparatorTextProps = {
    label: string;
};

export const SeparatorText: WidgetFunctionComponent<WidgetPropsMap["SeparatorText"]> = ({
    label,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    return (
        <separator-text
            label={label}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        />
    );
};
