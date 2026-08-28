/**
 * GENERATED FILE - DO NOT EDIT.
 * Source: react-native@0.87.0/Libraries/StyleSheet/flattenStyle.js
 * Source SHA-256: 27ca88d04c63b7713de710b4a66c579f61d2e1ede3ca279283097ff50b722cb1
 * Generator: scripts/extract-rn-fabric-renderer.ts
 */

/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of the React Native source tree.
 */

function flattenStyleArrayInto(result, styles) {
    for (let i = 0, styleLength = styles.length; i < styleLength; ++i) {
        const style = styles[i];
        if (style === null || typeof style !== "object") {
            continue;
        }
        if (Array.isArray(style)) {
            flattenStyleArrayInto(result, style);
            continue;
        }
        for (const key in style) {
            result[key] = style[key];
        }
    }
}
function flattenStyle(style) {
    if (style === null || typeof style !== "object") {
        return undefined;
    }
    if (!Array.isArray(style)) {
        return style;
    }
    const result = {};
    flattenStyleArrayInto(result, style);
    return result;
}
export default flattenStyle;
