import { useCallback, useRef, useState } from "react";
import { useWidgetRegistrationService } from "../../hooks";
import { WidgetFunctionComponent, WidgetPropsMap } from "./types";
import { TreeNode } from "./TreeNode";

export type TreeViewItem = {
    itemId: string;
    label: string;
    childItems?: TreeViewItem[];
};

type TreeViewItemRendererProps = {
    selectedItemIds: string[];
    item: TreeViewItem;
    onToggleItemSelection: (itemId: string, selected: boolean) => void;
};

const TreeViewItemRenderer = ({
    item,
    selectedItemIds,
    onToggleItemSelection,
}: TreeViewItemRendererProps) => {
    const selected = selectedItemIds.includes(item.itemId);

    const onClick = useCallback(() => {
        onToggleItemSelection(item.itemId, !selectedItemIds.includes(item.itemId));
    }, [onToggleItemSelection, item, selectedItemIds]);

    return (
        <TreeNode
            key={item.itemId}
            itemId={item.itemId}
            label={item.label}
            onClick={onClick}
            leaf={item.childItems === undefined}
            selected={selected}
            selectable
        >
            {item.childItems?.map((childItem) => (
                <TreeViewItemRenderer
                    key={childItem.itemId}
                    item={childItem}
                    selectedItemIds={selectedItemIds}
                    onToggleItemSelection={onToggleItemSelection}
                />
            ))}
        </TreeNode>
    );
};

export const TreeView: WidgetFunctionComponent<WidgetPropsMap["TreeView"]> = ({
    items,
    defaultSelectedItemIds,
    allowMultipleSelection,
    selectedItemIds,
    onToggleItemSelection,
    style,
}) => {
    const widgetRegistrationService = useWidgetRegistrationService();
    const idRef = useRef(widgetRegistrationService.generateId());

    const [localSelectedItemIds, setLocalSelectedItemIds] = useState(defaultSelectedItemIds ?? []);

    const handleItemSelectionToggled = useCallback(
        (itemId: string, selected: boolean) => {
            if (onToggleItemSelection) {
                onToggleItemSelection(itemId, selected);
            } else {
                if (selected && !localSelectedItemIds.includes(itemId)) {
                    setLocalSelectedItemIds((selection) => [...selection, itemId]);
                } else {
                    setLocalSelectedItemIds((selection) =>
                        selection.filter((item) => item !== itemId),
                    );
                }
            }
        },
        [onToggleItemSelection, localSelectedItemIds],
    );

    return (
        <>
            {items.map((item) => (
                <TreeViewItemRenderer
                    key={item.itemId}
                    item={item}
                    selectedItemIds={selectedItemIds ?? localSelectedItemIds}
                    onToggleItemSelection={handleItemSelectionToggled}
                />
            ))}
        </>
    );
};
