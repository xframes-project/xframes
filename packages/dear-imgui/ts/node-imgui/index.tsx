import * as React from "react";

import { ReactFabricProd } from "./ReactNativeWrapper";
import { WidgetRegistrationServiceContext } from "../src/lib/contexts/widgetRegistrationServiceContext";
import { WidgetRegistrationService } from "../src/lib/widgetRegistrationService";
import ReactNativePrivateInterface from "../src/lib/react-native/ReactNativePrivateInterface";
import { App } from "./App";
import { theme2 } from "../src/lib/stylesheet/themes";

const nodeImgui = require("../../cpp/node/build/Release/node-imgui.node");

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

nodeImgui.init(assetsBasePath, JSON.stringify(fontDefs), JSON.stringify(theme2));

const widgetRegistrationService = new WidgetRegistrationService(nodeImgui);

ReactNativePrivateInterface.nativeFabricUIManager.init(nodeImgui, widgetRegistrationService);

setTimeout(() => {
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

    nodeImgui.showDebugWindow();
}, 500);

let flag = true;
(function keepProcessRunning() {
    setTimeout(() => flag && keepProcessRunning(), 1000);
})();
