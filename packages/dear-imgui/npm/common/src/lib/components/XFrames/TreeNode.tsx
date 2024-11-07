import { useRef } from "react";
import { WidgetFunctionComponent, PropsWithChildren, WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export const TreeNode: WidgetFunctionComponent<PropsWithChildren & WidgetPropsMap["TreeNode"]> = ({
    children,
    itemId,
    label,
    onClick,
    open,
    defaultOpen,
    defaultSelected,
    selectable,
    selected,
    leaf,
    style,
    hoverStyle,
    activeStyle,
    disabledStyle,
}) => {
    const widgetRegistratonService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistratonService.generateId());

    return (
        <tree-node
            itemId={itemId}
            id={idRef.current}
            label={label}
            onClick={onClick}
            open={open}
            selectable={selectable}
            selected={selected}
            defaultOpen={defaultOpen}
            defaultSelected={defaultSelected}
            leaf={leaf}
            style={style}
            hoverStyle={hoverStyle}
            activeStyle={activeStyle}
            disabledStyle={disabledStyle}
        >
            {leaf ? null : children}
        </tree-node>
    );
};
