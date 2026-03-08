import { useCallback } from "react";
import { Primitive, WasmDeps } from "../components/XFrames/types";

export const useXFramesWasm = (ReactNativePrivateInterface: any): WasmDeps => {
    const onTextChange = useCallback((id: number, value: string) => {
        const rootNodeID = id;
        const topLevelType = "onChange";
        const nativeEventParam = { value };

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
            rootNodeID,
            topLevelType,
            nativeEventParam,
        );
    }, []);

    const onComboChange = useCallback((id: number, value: number) => {
        const rootNodeID = id;
        const topLevelType = "onChange";
        const nativeEventParam = { value };

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
            rootNodeID,
            topLevelType,
            nativeEventParam,
        );
    }, []);

    const onNumericValueChange = useCallback((id: number, value: number) => {
        const rootNodeID = id;
        const topLevelType = "onChange";
        const nativeEventParam = { value };

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
            rootNodeID,
            topLevelType,
            nativeEventParam,
        );
    }, []);

    const onMultiValueChange = useCallback((id: number, values: Primitive[]) => {
        const rootNodeID = id;
        const topLevelType = "onChange";
        const nativeEventParam = { values };

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
            rootNodeID,
            topLevelType,
            nativeEventParam,
        );
    }, []);

    const onBooleanValueChange = useCallback((id: number, value: boolean) => {
        const rootNodeID = id;
        const topLevelType = "onChange";
        const nativeEventParam = { value };

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
            rootNodeID,
            topLevelType,
            nativeEventParam,
        );
    }, []);

    const onClick = useCallback((id: number) => {
        const rootNodeID = id;
        const topLevelType = "onClick";

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(rootNodeID, topLevelType, {
            value: "clicked",
        });
    }, []);

    const onTableSort = useCallback((id: number, columnIndex: number, sortDirection: number) => {
        const rootNodeID = id;
        const topLevelType = "onSort";
        const nativeEventParam = { columnIndex, sortDirection };

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
            rootNodeID,
            topLevelType,
            nativeEventParam,
        );
    }, []);

    const onTableFilter = useCallback((id: number, columnIndex: number, filterText: string) => {
        const rootNodeID = id;
        const topLevelType = "onFilter";
        const nativeEventParam = { columnIndex, filterText };

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
            rootNodeID,
            topLevelType,
            nativeEventParam,
        );
    }, []);

    const onTableRowClick = useCallback((id: number, rowIndex: number) => {
        const rootNodeID = id;
        const topLevelType = "onRowClick";
        const nativeEventParam = { rowIndex };

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
            rootNodeID,
            topLevelType,
            nativeEventParam,
        );
    }, []);

    const onTableItemAction = useCallback((id: number, rowIndex: number, actionId: string) => {
        const rootNodeID = id;
        const topLevelType = "onItemAction";
        const nativeEventParam = { rowIndex, actionId };

        ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
            rootNodeID,
            topLevelType,
            nativeEventParam,
        );
    }, []);

    return {
        eventHandlers: {
            onTextChange,
            onComboChange,
            onNumericValueChange,
            onMultiValueChange,
            onBooleanValueChange,
            onClick,
            onTableSort,
            onTableFilter,
            onTableRowClick,
            onTableItemAction,
        },
    };
};
