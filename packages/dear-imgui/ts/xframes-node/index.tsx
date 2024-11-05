import * as React from "react";

import { WidgetRegistrationServiceContext } from "../src/lib/contexts/widgetRegistrationServiceContext";
import { WidgetRegistrationService } from "../src/lib/widgetRegistrationService";
import ReactNativePrivateInterface from "../src/lib/react-native/ReactNativePrivateInterface";
import { App } from "./App";
import { theme2 } from "../src/lib/stylesheet/themes";
import { Primitive } from "../src/lib/components/XFrames/types";
import ReactFabricProdInitialiser from "../src/lib/react-native/ReactFabric-prod";

export const ReactFabricProd = ReactFabricProdInitialiser(ReactNativePrivateInterface);

const xframes = require("../../cpp/node/xframes");

const fontDefs: any = {
    defs: [
        { name: "roboto-regular", sizes: [16, 18, 20, 24, 28, 32, 36, 48] },
        { name: "roboto-bold", sizes: [16, 18, 20, 24, 28, 32, 36, 48] },
        // { name: "roboto-light", sizes: [12, 14, 16, 18, 20, 24, 28, 32, 36, 48] },
        { name: "roboto-mono-regular", sizes: [14, 16] },
    ]
        .map(({ name, sizes }) => sizes.map((size) => ({ name, size })))
        .flat(),
};

const assetsBasePath = "../assets";

const onInit = () => {
    ReactFabricProd.render(
        <WidgetRegistrationServiceContext.Provider value={widgetRegistrationService}>
            <App />
        </WidgetRegistrationServiceContext.Provider>,
        0, // containerTag,
        () => {
            // console.log("initialised");
        },
        1,
    );
};

const onTextChange = (id: number, value: string) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { value };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
        rootNodeID,
        topLevelType,
        nativeEventParam,
    );
};

const onComboChange = (id: number, value: number) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { value };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
        rootNodeID,
        topLevelType,
        nativeEventParam,
    );
};

const onNumericValueChange = (id: number, value: number) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { value };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
        rootNodeID,
        topLevelType,
        nativeEventParam,
    );
};

const onMultiValueChange = (id: number, values: Primitive[]) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { values };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
        rootNodeID,
        topLevelType,
        nativeEventParam,
    );
};

const onBooleanValueChange = (id: number, value: boolean) => {
    const rootNodeID = id;
    const topLevelType = "onChange";
    const nativeEventParam = { value };

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(
        rootNodeID,
        topLevelType,
        nativeEventParam,
    );
};

const onClick = (id: number) => {
    const rootNodeID = id;
    const topLevelType = "onClick";

    ReactNativePrivateInterface.nativeFabricUIManager.dispatchEvent(rootNodeID, topLevelType, {
        value: "clicked",
    });
};

xframes.init(
    assetsBasePath,
    JSON.stringify(fontDefs),
    JSON.stringify(theme2),
    onInit,
    onTextChange,
    onComboChange,
    onNumericValueChange,
    onBooleanValueChange,
    onMultiValueChange,
    onClick,
);

const widgetRegistrationService = new WidgetRegistrationService(xframes);

ReactNativePrivateInterface.nativeFabricUIManager.init(xframes, widgetRegistrationService);

// xframes.showDebugWindow();

let flag = true;
(function keepProcessRunning() {
    setTimeout(() => flag && keepProcessRunning(), 1000);
})();
