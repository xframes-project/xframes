/**
 * GENERATED FILE - DO NOT EDIT.
 * Source: react-native@0.87.0/Libraries/Utilities/differ/deepDiffer.js
 * Source SHA-256: c409a6ee94932ffc56c3dcfd68420f73f2b5c45cb695417eabf39c038e226fa5
 * Generator: scripts/extract-rn-fabric-renderer.ts
 */

/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of the React Native source tree.
 */

let logListeners;
function unstable_setLogListeners(listeners) {
    logListeners = listeners;
}
function deepDiffer(one, two, maxDepthOrOptions = -1, maybeOptions) {
    const options = typeof maxDepthOrOptions === "number" ? maybeOptions : maxDepthOrOptions;
    const maxDepth = typeof maxDepthOrOptions === "number" ? maxDepthOrOptions : -1;
    if (maxDepth === 0) {
        return true;
    }
    if (one === two) {
        return false;
    }
    if (typeof one === "function" && typeof two === "function") {
        let unsafelyIgnoreFunctions = options?.unsafelyIgnoreFunctions;
        if (unsafelyIgnoreFunctions == null) {
            if (
                logListeners &&
                logListeners.onDifferentFunctionsIgnored &&
                (!options || !("unsafelyIgnoreFunctions" in options))
            ) {
                logListeners.onDifferentFunctionsIgnored(one.name, two.name);
            }
            unsafelyIgnoreFunctions = true;
        }
        return !unsafelyIgnoreFunctions;
    }
    if (typeof one !== "object" || one === null) {
        return one !== two;
    }
    if (typeof two !== "object" || two === null) {
        return true;
    }
    if (one.constructor !== two.constructor) {
        return true;
    }
    if (Array.isArray(one)) {
        const len = one.length;
        if (two.length !== len) {
            return true;
        }
        for (let ii = 0; ii < len; ii++) {
            if (deepDiffer(one[ii], two[ii], maxDepth - 1, options)) {
                return true;
            }
        }
    } else {
        for (const key in one) {
            if (deepDiffer(one[key], two[key], maxDepth - 1, options)) {
                return true;
            }
        }
        for (const twoKey in two) {
            if (one[twoKey] === undefined && two[twoKey] !== undefined) {
                return true;
            }
        }
    }
    return false;
}
deepDiffer.unstable_setLogListeners = unstable_setLogListeners;
export default deepDiffer;
