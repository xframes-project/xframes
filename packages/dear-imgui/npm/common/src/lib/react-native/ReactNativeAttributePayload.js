/**
 * GENERATED FILE - DO NOT EDIT.
 * Source: react-native@0.87.0/Libraries/ReactNative/ReactFabricPublicInstance/ReactNativeAttributePayload.js
 * Source SHA-256: 7c44c2c74734959dc43838bc396873bf39c337a433c3b02a53965bfe1ed92611
 * Generator: scripts/extract-rn-fabric-renderer.ts
 */

/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of the React Native source tree.
 */

import flattenStyle from "./flattenStyle.js";
import deepDiffer from "./deepDiffer.js";
const emptyObject = {};
let removedKeys = null;
let removedKeyCount = 0;
const deepDifferOptions = {
    unsafelyIgnoreFunctions: true,
};
function defaultDiffer(prevProp, nextProp) {
    if (typeof nextProp !== "object" || nextProp === null) {
        return true;
    } else {
        return deepDiffer(prevProp, nextProp, deepDifferOptions);
    }
}
function restoreDeletedValuesInNestedArray(updatePayload, node, validAttributes) {
    if (Array.isArray(node)) {
        let i = node.length;
        while (i-- && removedKeyCount > 0) {
            restoreDeletedValuesInNestedArray(updatePayload, node[i], validAttributes);
        }
    } else if (node && removedKeyCount > 0) {
        const obj = node;
        for (const propKey in removedKeys) {
            if (!removedKeys[propKey]) {
                continue;
            }
            let nextProp = obj[propKey];
            if (nextProp === undefined) {
                continue;
            }
            const attributeConfig = validAttributes[propKey];
            if (!attributeConfig) {
                continue;
            }
            if (typeof nextProp === "function") {
                nextProp = true;
            }
            if (typeof nextProp === "undefined") {
                nextProp = null;
            }
            if (typeof attributeConfig !== "object") {
                updatePayload[propKey] = nextProp;
            } else if (
                typeof attributeConfig.diff === "function" ||
                typeof attributeConfig.process === "function"
            ) {
                const nextValue =
                    typeof attributeConfig.process === "function"
                        ? attributeConfig.process(nextProp)
                        : nextProp;
                updatePayload[propKey] = nextValue;
            }
            removedKeys[propKey] = false;
            removedKeyCount--;
        }
    }
}
function diffNestedArrayProperty(updatePayload, prevArray, nextArray, validAttributes) {
    const minLength = prevArray.length < nextArray.length ? prevArray.length : nextArray.length;
    let i;
    for (i = 0; i < minLength; i++) {
        updatePayload = diffNestedProperty(
            updatePayload,
            prevArray[i],
            nextArray[i],
            validAttributes,
        );
    }
    for (; i < prevArray.length; i++) {
        updatePayload = clearNestedProperty(updatePayload, prevArray[i], validAttributes);
    }
    for (; i < nextArray.length; i++) {
        const nextProp = nextArray[i];
        if (!nextProp) {
            continue;
        }
        updatePayload = addNestedProperty(updatePayload, nextProp, validAttributes);
    }
    return updatePayload;
}
function diffNestedProperty(updatePayload, prevProp, nextProp, validAttributes) {
    if (!updatePayload && prevProp === nextProp) {
        return updatePayload;
    }
    if (!prevProp || !nextProp) {
        if (nextProp) {
            return addNestedProperty(updatePayload, nextProp, validAttributes);
        }
        if (prevProp) {
            return clearNestedProperty(updatePayload, prevProp, validAttributes);
        }
        return updatePayload;
    }
    if (!Array.isArray(prevProp) && !Array.isArray(nextProp)) {
        return diffProperties(updatePayload, prevProp, nextProp, validAttributes);
    }
    if (Array.isArray(prevProp) && Array.isArray(nextProp)) {
        return diffNestedArrayProperty(updatePayload, prevProp, nextProp, validAttributes);
    }
    if (Array.isArray(prevProp)) {
        return diffProperties(updatePayload, flattenStyle(prevProp), nextProp, validAttributes);
    }
    return diffProperties(updatePayload, prevProp, flattenStyle(nextProp), validAttributes);
}
function clearNestedProperty(updatePayload, prevProp, validAttributes) {
    if (!prevProp) {
        return updatePayload;
    }
    if (!Array.isArray(prevProp)) {
        return clearProperties(updatePayload, prevProp, validAttributes);
    }
    for (let i = 0; i < prevProp.length; i++) {
        updatePayload = clearNestedProperty(updatePayload, prevProp[i], validAttributes);
    }
    return updatePayload;
}
function diffProperties(updatePayload, prevProps, nextProps, validAttributes) {
    let attributeConfig;
    let nextProp;
    let prevProp;
    for (const propKey in nextProps) {
        attributeConfig = validAttributes[propKey];
        if (!attributeConfig) {
            continue;
        }
        prevProp = prevProps[propKey];
        nextProp = nextProps[propKey];
        if (typeof nextProp === "function") {
            const attributeConfigHasProcess =
                typeof attributeConfig === "object" &&
                typeof attributeConfig.process === "function";
            if (!attributeConfigHasProcess) {
                nextProp = true;
                if (typeof prevProp === "function") {
                    prevProp = true;
                }
            }
        }
        if (typeof nextProp === "undefined") {
            nextProp = null;
            if (typeof prevProp === "undefined") {
                prevProp = null;
            }
        }
        if (removedKeys) {
            removedKeys[propKey] = false;
        }
        if (updatePayload && updatePayload[propKey] !== undefined) {
            if (typeof attributeConfig !== "object") {
                updatePayload[propKey] = nextProp;
            } else if (
                typeof attributeConfig.diff === "function" ||
                typeof attributeConfig.process === "function"
            ) {
                const nextValue =
                    typeof attributeConfig.process === "function"
                        ? attributeConfig.process(nextProp)
                        : nextProp;
                updatePayload[propKey] = nextValue;
            }
            continue;
        }
        if (prevProp === nextProp) {
            continue;
        }
        if (typeof attributeConfig !== "object") {
            if (defaultDiffer(prevProp, nextProp)) {
                (updatePayload || (updatePayload = {}))[propKey] = nextProp;
            }
        } else if (
            typeof attributeConfig.diff === "function" ||
            typeof attributeConfig.process === "function"
        ) {
            const shouldUpdate =
                prevProp === undefined ||
                (typeof attributeConfig.diff === "function"
                    ? attributeConfig.diff(prevProp, nextProp)
                    : defaultDiffer(prevProp, nextProp));
            if (shouldUpdate) {
                const nextValue =
                    typeof attributeConfig.process === "function"
                        ? attributeConfig.process(nextProp)
                        : nextProp;
                (updatePayload || (updatePayload = {}))[propKey] = nextValue;
            }
        } else {
            removedKeys = null;
            removedKeyCount = 0;
            updatePayload = diffNestedProperty(updatePayload, prevProp, nextProp, attributeConfig);
            if (removedKeyCount > 0 && updatePayload) {
                restoreDeletedValuesInNestedArray(updatePayload, nextProp, attributeConfig);
                removedKeys = null;
            }
        }
    }
    for (const propKey in prevProps) {
        if (nextProps[propKey] !== undefined) {
            continue;
        }
        attributeConfig = validAttributes[propKey];
        if (!attributeConfig) {
            continue;
        }
        if (updatePayload && updatePayload[propKey] !== undefined) {
            continue;
        }
        prevProp = prevProps[propKey];
        if (prevProp === undefined) {
            continue;
        }
        if (
            typeof attributeConfig !== "object" ||
            typeof attributeConfig.diff === "function" ||
            typeof attributeConfig.process === "function"
        ) {
            (updatePayload || (updatePayload = {}))[propKey] = null;
            if (!removedKeys) {
                removedKeys = {};
            }
            if (!removedKeys[propKey]) {
                removedKeys[propKey] = true;
                removedKeyCount++;
            }
        } else {
            updatePayload = clearNestedProperty(updatePayload, prevProp, attributeConfig);
        }
    }
    return updatePayload;
}
function addNestedProperty(payload, props, validAttributes) {
    if (Array.isArray(props)) {
        for (let i = 0; i < props.length; i++) {
            payload = addNestedProperty(payload, props[i], validAttributes);
        }
        return payload;
    }
    for (const propKey in props) {
        const prop = props[propKey];
        const attributeConfig = validAttributes[propKey];
        if (attributeConfig == null) {
            continue;
        }
        let newValue;
        if (prop === undefined) {
            if (payload && payload[propKey] !== undefined) {
                newValue = null;
            } else {
                continue;
            }
        } else if (typeof attributeConfig === "object") {
            if (typeof attributeConfig.process === "function") {
                newValue = attributeConfig.process(prop);
            } else if (typeof attributeConfig.diff === "function") {
                newValue = prop;
            }
        } else {
            if (typeof prop === "function") {
                newValue = true;
            } else {
                newValue = prop;
            }
        }
        if (newValue !== undefined) {
            if (!payload) {
                payload = {};
            }
            payload[propKey] = newValue;
            continue;
        }
        payload = addNestedProperty(payload, prop, attributeConfig);
    }
    return payload;
}
function clearProperties(updatePayload, prevProps, validAttributes) {
    return diffProperties(updatePayload, prevProps, emptyObject, validAttributes);
}
export function create(props, validAttributes) {
    return addNestedProperty(null, props, validAttributes);
}
export function diff(prevProps, nextProps, validAttributes) {
    return diffProperties(null, prevProps, nextProps, validAttributes);
}
