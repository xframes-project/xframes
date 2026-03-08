import * as React from "react";
import {
  Primitive,
  ReactFabricProdInitialiser,
  ReactNativePrivateInterface,
  WidgetRegistrationService,
  WidgetRegistrationServiceContext,
} from "@xframes/common";

const xframes = require("./xframes.node");

export const ReactFabricProd = ReactFabricProdInitialiser(
  ReactNativePrivateInterface
);

export const render = (
  EntryPointComponent: () => JSX.Element,
  assetsBasePath: string,
  fontDefs: any,
  theme: any
) => {
  const onInit = () => {
    ReactFabricProd.render(
      <WidgetRegistrationServiceContext.Provider
        value={widgetRegistrationService}
      >
        <EntryPointComponent />
      </WidgetRegistrationServiceContext.Provider>,
      0, // containerTag,
      () => {
        // console.log("initialised");
      },
      1
    );
  };

  const onTextChange = (id: number, value: string) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { value };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      nativeEventParam
    );
  };

  const onComboChange = (id: number, value: number) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { value };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      nativeEventParam
    );
  };

  const onNumericValueChange = (id: number, value: number) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { value };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      nativeEventParam
    );
  };

  const onMultiValueChange = (id: number, values: Primitive[]) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { values };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      nativeEventParam
    );
  };

  const onBooleanValueChange = (id: number, value: boolean) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { value };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      nativeEventParam
    );
  };

  const onClick = (id: number) => {
    const rootNodeID = id;
    const topLevelType = "onClick";

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      {
        value: "clicked",
      }
    );
  };

  const onTableSort = (id: number, columnIndex: number, sortDirection: number) => {
    const rootNodeID = id;
    const topLevelType = "onSort";
    const nativeEventParam = { columnIndex, sortDirection };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      nativeEventParam
    );
  };

  const onTableFilter = (id: number, columnIndex: number, filterText: string) => {
    const rootNodeID = id;
    const topLevelType = "onFilter";
    const nativeEventParam = { columnIndex, filterText };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      nativeEventParam
    );
  };

  const onTableRowClick = (id: number, rowIndex: number) => {
    const rootNodeID = id;
    const topLevelType = "onRowClick";
    const nativeEventParam = { rowIndex };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      nativeEventParam
    );
  };

  const onTableItemAction = (id: number, rowIndex: number, actionId: string) => {
    const rootNodeID = id;
    const topLevelType = "onItemAction";
    const nativeEventParam = { rowIndex, actionId };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
      rootNodeID,
      topLevelType,
      nativeEventParam
    );
  };

  xframes.init(
    assetsBasePath,
    JSON.stringify(fontDefs),
    JSON.stringify(theme),
    onInit,
    onTextChange,
    onComboChange,
    onNumericValueChange,
    onBooleanValueChange,
    onMultiValueChange,
    onClick,
    onTableSort,
    onTableFilter,
    onTableRowClick,
    onTableItemAction
  );

  const widgetRegistrationService = new WidgetRegistrationService(xframes);

  ReactNativePrivateInterface.nativeFabricUIManager.init(
    xframes,
    widgetRegistrationService
  );

  let flag = true;
  (function keepProcessRunning() {
    setTimeout(() => flag && keepProcessRunning(), 1000);
  })();
};
