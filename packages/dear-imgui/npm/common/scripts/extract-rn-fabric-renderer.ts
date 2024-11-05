import path from "node:path";
import * as prettier from "prettier";
import { existsSync, readFileSync, writeFileSync } from "fs";

const pathToReactFabricProd = path.join(
    "node_modules",
    "react-native",
    "Libraries",
    "Renderer",
    "implementations",
    "ReactFabric-prod.js",
);

const destPath = path.join("src", "lib", "react-native", "ReactFabric-prod.js");

const runner = async () => {
    if (existsSync(pathToReactFabricProd)) {
        let reactFabricProdContents = readFileSync(pathToReactFabricProd, { encoding: "utf8" });

        reactFabricProdContents = reactFabricProdContents.replace('"use strict";', "");

        reactFabricProdContents = reactFabricProdContents.replace(
            'require("react-native/Libraries/ReactPrivate/ReactNativePrivateInitializeCore");',
            "",
        );

        reactFabricProdContents = reactFabricProdContents.replace(
            'var ReactNativePrivateInterface = require("react-native/Libraries/ReactPrivate/ReactNativePrivateInterface"),',
            "",
        );

        reactFabricProdContents = reactFabricProdContents.replace('React = require("react"),', "");

        reactFabricProdContents = reactFabricProdContents.replace(
            'Scheduler = require("scheduler");',
            "",
        );

        reactFabricProdContents = reactFabricProdContents.replace(
            "var _nativeFabricUIManage = nativeFabricUIManager",
            "var _nativeFabricUIManage = ReactNativePrivateInterface.nativeFabricUIManager",
        );

        reactFabricProdContents = reactFabricProdContents.replaceAll("exports.", "obj.");

        reactFabricProdContents = `
    import * as React from "react";
    import Scheduler from "scheduler";

    export default (ReactNativePrivateInterface) => {

        const obj = {};

    ${reactFabricProdContents}

        return obj;
    };

    `;

        reactFabricProdContents = await prettier.format(reactFabricProdContents, {
            trailingComma: "all",
            tabWidth: 4,
            semi: true,
            singleQuote: false,
            printWidth: 100,
            parser: "babel",
        });

        writeFileSync(destPath, reactFabricProdContents, {
            encoding: "utf8",
        });
    }
};

runner();
