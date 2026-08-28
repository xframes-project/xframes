import { createHash } from "node:crypto";
import { createRequire } from "node:module";
import path from "node:path";
import { readFileSync, writeFileSync } from "node:fs";
import { transformSync, types as t } from "@babel/core";
import type { NodePath } from "@babel/traverse";
import flowStripTypes from "@babel/plugin-transform-flow-strip-types";
import syntaxHermesParser from "babel-plugin-syntax-hermes-parser";
import * as prettier from "prettier";

type EmbeddedSource = {
    sourcePath: string;
    destination: string;
    transform: "renderer" | "flow";
};

const packageRoot = path.resolve(__dirname, "..");
const localRequire = createRequire(path.join(packageRoot, "package.json"));
const reactNativePackageJsonPath = localRequire.resolve("react-native/package.json");
const reactNativeRoot = path.dirname(reactNativePackageJsonPath);
const reactNativePackage = JSON.parse(readFileSync(reactNativePackageJsonPath, "utf8"));
const outputRoot = path.join(packageRoot, "src", "lib", "react-native");

const embeddedSources: EmbeddedSource[] = [
    {
        sourcePath: "Libraries/Renderer/implementations/ReactFabric-dev.js",
        destination: "ReactFabric-dev.js",
        transform: "renderer",
    },
    {
        sourcePath: "Libraries/Renderer/implementations/ReactFabric-prod.js",
        destination: "ReactFabric-prod.js",
        transform: "renderer",
    },
    {
        sourcePath:
            "Libraries/ReactNative/ReactFabricPublicInstance/ReactNativeAttributePayload.js",
        destination: "ReactNativeAttributePayload.js",
        transform: "flow",
    },
    {
        sourcePath: "Libraries/StyleSheet/flattenStyle.js",
        destination: "flattenStyle.js",
        transform: "flow",
    },
    {
        sourcePath: "Libraries/Utilities/differ/deepDiffer.js",
        destination: "deepDiffer.js",
        transform: "flow",
    },
];

const rendererModules = new Set([
    "react-native/Libraries/ReactPrivate/ReactNativePrivateInitializeCore",
    "react-native/Libraries/ReactPrivate/ReactNativePrivateInterface",
    "react",
    "scheduler",
]);

const helperImports = new Map([
    ["../../StyleSheet/flattenStyle", "./flattenStyle.js"],
    ["../../Utilities/differ/deepDiffer", "./deepDiffer.js"],
]);

const sourceHash = (source: string) => createHash("sha256").update(source).digest("hex");

const upstreamLicenseNotice = (source: string, sourcePath: string) => {
    if (
        !source.includes("Copyright (c) Meta Platforms, Inc. and affiliates.") ||
        !source.includes("This source code is licensed under the MIT license")
    ) {
        throw new Error(`React Native license notice changed or is missing in ${sourcePath}`);
    }

    return `
/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of the React Native source tree.
 */
`;
};

const generatedBanner = (sourcePath: string, hash: string) => `
/**
 * GENERATED FILE - DO NOT EDIT.
 * Source: react-native@${reactNativePackage.version}/${sourcePath}
 * Source SHA-256: ${hash}
 * Generator: scripts/extract-rn-fabric-renderer.ts
 */
`;

const transformRenderer = (source: string, sourcePath: string) => {
    const removedModules = new Map<string, number>();
    const exportedNames = new Set<string>();

    const result = transformSync(source, {
        ast: false,
        babelrc: false,
        code: true,
        comments: true,
        compact: false,
        configFile: false,
        filename: sourcePath,
        sourceType: "script",
        plugins: [
            () => ({
                visitor: {
                    Program(programPath: NodePath<t.Program>) {
                        programPath.node.directives = programPath.node.directives.filter(
                            ({ value }) => value.value !== "use strict",
                        );
                    },
                    CallExpression(callPath: NodePath<t.CallExpression>) {
                        const { node } = callPath;
                        if (
                            !t.isIdentifier(node.callee, { name: "require" }) ||
                            node.arguments.length !== 1 ||
                            !t.isStringLiteral(node.arguments[0])
                        ) {
                            return;
                        }

                        const moduleName = node.arguments[0].value;
                        if (!rendererModules.has(moduleName)) {
                            throw callPath.buildCodeFrameError(
                                `Unsupported React Fabric dependency: ${moduleName}`,
                            );
                        }

                        removedModules.set(moduleName, (removedModules.get(moduleName) ?? 0) + 1);

                        if (callPath.parentPath.isExpressionStatement()) {
                            callPath.parentPath.remove();
                            return;
                        }

                        if (!callPath.parentPath.isVariableDeclarator()) {
                            throw callPath.buildCodeFrameError(
                                `Unsupported require() location for ${moduleName}`,
                            );
                        }

                        callPath.parentPath.remove();
                    },
                    AssignmentExpression(assignmentPath: NodePath<t.AssignmentExpression>) {
                        const { left } = assignmentPath.node;
                        if (
                            t.isMemberExpression(left) &&
                            !left.computed &&
                            t.isIdentifier(left.object, { name: "exports" }) &&
                            t.isIdentifier(left.property)
                        ) {
                            exportedNames.add(left.property.name);
                            left.object = t.identifier("obj");
                        }
                    },
                },
            }),
        ],
    });

    for (const moduleName of rendererModules.keys()) {
        if (removedModules.get(moduleName) !== 1) {
            throw new Error(
                `Expected exactly one ${moduleName} import in ${sourcePath}; found ${removedModules.get(moduleName) ?? 0}`,
            );
        }
    }

    if (!exportedNames.has("render") || !exportedNames.has("stopSurface")) {
        throw new Error(
            `React Fabric exports changed in ${sourcePath}; found: ${[...exportedNames].sort().join(", ")}`,
        );
    }

    if (!result?.code) {
        throw new Error(`Babel produced no output for ${sourcePath}`);
    }

    if (/\brequire\s*\(/.test(result.code)) {
        throw new Error(`Unresolved require() call remains in ${sourcePath}`);
    }
    if (/\bexports\./.test(result.code)) {
        throw new Error(`Unresolved CommonJS export remains in ${sourcePath}`);
    }

    return `
${generatedBanner(sourcePath, sourceHash(source))}
${upstreamLicenseNotice(source, sourcePath)}
import * as React from "react";
import * as Scheduler from "scheduler";

export default (ReactNativePrivateInterface) => {
    const nativeFabricUIManager = ReactNativePrivateInterface.nativeFabricUIManager;
    const obj = {};
    ${sourcePath.endsWith("-dev.js") ? "const __DEV__ = true;" : ""}

${result.code}

    return obj;
};
`;
};

const transformFlowHelper = (source: string, sourcePath: string) => {
    const result = transformSync(source, {
        ast: false,
        babelrc: false,
        code: true,
        comments: true,
        compact: false,
        configFile: false,
        filename: sourcePath,
        plugins: [
            syntaxHermesParser,
            flowStripTypes,
            () => ({
                visitor: {
                    Program(programPath: NodePath<t.Program>) {
                        programPath.node.directives = programPath.node.directives.filter(
                            ({ value }) => value.value !== "use strict",
                        );
                    },
                    ImportDeclaration(importPath: NodePath<t.ImportDeclaration>) {
                        const replacement = helperImports.get(importPath.node.source.value);
                        if (!replacement) {
                            throw importPath.buildCodeFrameError(
                                `Unsupported embedded helper dependency: ${importPath.node.source.value}`,
                            );
                        }
                        importPath.node.source = t.stringLiteral(replacement);
                    },
                },
            }),
        ],
    });

    if (!result?.code) {
        throw new Error(`Babel produced no output for ${sourcePath}`);
    }

    const externalImports = [...result.code.matchAll(/from\s+["']([^"']+)["']/g)]
        .map((match) => match[1])
        .filter((moduleName) => !moduleName.startsWith("./"));
    if (externalImports.length > 0) {
        throw new Error(
            `Unsupported helper imports remain in ${sourcePath}: ${externalImports.join(", ")}`,
        );
    }

    return `${generatedBanner(sourcePath, sourceHash(source))}\n${upstreamLicenseNotice(source, sourcePath)}\n${result.code}\n`;
};

const format = (source: string) =>
    prettier.format(source, {
        trailingComma: "all",
        tabWidth: 4,
        semi: true,
        singleQuote: false,
        printWidth: 100,
        parser: "babel",
    });

const run = async () => {
    const checkOnly = process.argv.includes("--check");
    let stale = false;

    for (const embeddedSource of embeddedSources) {
        const absoluteSourcePath = path.join(reactNativeRoot, embeddedSource.sourcePath);
        const absoluteDestination = path.join(outputRoot, embeddedSource.destination);
        const source = readFileSync(absoluteSourcePath, "utf8");
        const transformed =
            embeddedSource.transform === "renderer"
                ? transformRenderer(source, embeddedSource.sourcePath)
                : transformFlowHelper(source, embeddedSource.sourcePath);
        const formatted = await format(transformed);

        if (checkOnly) {
            if (readFileSync(absoluteDestination, "utf8") !== formatted) {
                console.error(`${embeddedSource.destination} is stale`);
                stale = true;
            }
        } else {
            writeFileSync(absoluteDestination, formatted, "utf8");
            console.log(
                `Embedded react-native@${reactNativePackage.version}/${embeddedSource.sourcePath}`,
            );
        }
    }

    if (stale) {
        throw new Error(
            "Embedded React Native files are stale. Run npm run extract-rn-fabric-renderer and commit them.",
        );
    }
};

run().catch((error: unknown) => {
    console.error(error);
    process.exitCode = 1;
});
