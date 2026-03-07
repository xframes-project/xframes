import { useMemo, useRef } from "react";
import {
    PropsWithChildren,
    WidgetFunctionComponent,
    WidgetPropsMap,
    WidgetReactElement,
} from "./types";
import { TabItem } from "./TabItem";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export const TabBar: WidgetFunctionComponent<PropsWithChildren & WidgetPropsMap["TabBar"]> = ({
    children,
    reorderable,
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
                (child): child is WidgetReactElement<"TabItem"> => child != null && child.type === TabItem,
            );
        }

        return [];
    }, [children]);

    return (
        <tab-bar
            id={idRef.current}
            reorderable={reorderable}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {tabs}
        </tab-bar>
    );
};
