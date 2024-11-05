import { useMemo, useRef } from "react";
import { useWidgetRegistrationService } from "../../hooks";
import {
    PropsWithChildren,
    WidgetFunctionComponent,
    WidgetPropsMap,
    WidgetReactElement,
} from "./types";
import { TabItem } from "./TabItem";

export const TabBar: WidgetFunctionComponent<PropsWithChildren & WidgetPropsMap["TabBar"]> = ({
    children,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    const widgetRegistratonService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistratonService.generateId());

    const tabs = useMemo(() => {
        if (children) {
            const localChildren = Array.isArray(children) ? children : [children];

            return localChildren.filter(
                (child): child is WidgetReactElement<"TabItem"> => child.type === TabItem,
            );
        }

        return [];
    }, [children]);

    return (
        <tab-bar
            id={idRef.current}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {tabs}
        </tab-bar>
    );
};
