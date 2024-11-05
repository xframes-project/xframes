import { PropsWithChildren, WidgetFunctionComponent, WidgetPropsMap } from "./types";

export const ItemTooltip: WidgetFunctionComponent<
    PropsWithChildren & WidgetPropsMap["ItemTooltip"]
> = ({ children, style, hoverStyle, activeStyle, disabledStyle }) => {
    return (
        <item-tooltip
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {children}
        </item-tooltip>
    );
};
