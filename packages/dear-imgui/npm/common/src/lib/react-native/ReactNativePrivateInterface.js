import NativeFabricUIManager from "./nativeFabricUiManager.ts";
import deepDiffer from "./deepDiffer.js";

const uiManager = new NativeFabricUIManager();

const commonAttributes = ["id", "style", "hoverStyle", "activeStyle", "disabledStyle"];

const attributesForElements = {
    "bullet-text": ["type", "text"],
    "di-button": ["onClick", "label", "size"],
    checkbox: ["defaultChecked", "label", "onChange"],
    child: ["defaultChecked", "label", "onChange"],
    "clipped-multi-line-text-renderer": [],
    "collapsing-header": ["label"],
    combo: ["placeholder", "options", "optionsList", "initialSelectedIndex", "onChange"],
    "disabled-text": ["text"],
    "di-window": ["title", "width", "number"],
    group: [],
    "help-marker": ["text"],
    "di-image": ["url", "width", "number"],
    indent: [],
    "input-text": ["defaultValue", "hint", "onChange"],
    "item-tooltip": [],
    "map-view": [],
    "multi-slider": [
        "numValues",
        "label",
        "defaultValues",
        "min",
        "max",
        "decimalDigits",
        "onChange",
    ],
    "plot-line": [
        "xAxisDecimalDigits",
        "yAxisDecimalDigits",
        "xAxisScale",
        "yAxisScale",
        "axisAutoFit",
        "markerStyle",
    ],
    "plot-candlestick": ["axisAutoFit", "bullColor", "bearColor"],
    separator: [],
    "separator-text": ["label"],
    slider: ["sliderType", "label", "defaultValue", "min", "max", "onChange"],
    "tab-bar": ["label"],
    "tab-item": ["label", "onOpenChange"],
    "di-table": ["columns", "initialData", "clipRows"],
    "text-wrap": ["width"],
    "tree-node": [
        "itemId",
        "onClick",
        "leaf",
        "open",
        "defaultOpen",
        "selected",
        "defaultSelected",
        "selectable",
        "label",
    ],
    "unformatted-text": ["text"],

    node: ["root", "cull", "trackMouseClickEvents"],
};

const attributesForElementsMap = Object.fromEntries(
    Object.entries(attributesForElements).map(([key, attributes]) => {
        const attributeMap = attributes.reduce((acc, item) => {
            acc[item] = true;

            return acc;
        }, {});

        commonAttributes.forEach((commonAttribute) => {
            attributeMap[commonAttribute] = true;
        });

        return [key, { validAttributes: attributeMap }];
    }),
);

export default {
    createPublicInstance(current, renderLanes, workInProgress) {
        // console.log("createPublicInstance", current, renderLanes, workInProgress);

        return {};
    },
    get BatchedBridge() {
        return {};
    },
    get ExceptionsManager() {
        return {};
    },
    get Platform() {
        return {};
    },
    get ReactNativeViewConfigRegistry() {
        return {
            customBubblingEventTypes: {},
            customDirectEventTypes: {
                onChange: { registrationName: "onChange" },
                onClick: { registrationName: "onClick" },
            },
            get(elementType, ...unknownArgs) {
                if (attributesForElementsMap[elementType] === undefined) {
                    console.log(`Unrecognised element type: ${elementType}`);
                }

                return attributesForElementsMap[elementType] !== undefined
                    ? attributesForElementsMap[elementType]
                    : { validAttributes: {} };
            },
        };
    },
    get TextInputState() {
        return {};
    },
    get nativeFabricUIManager() {
        return uiManager;
    },
    // TODO: Remove when React has migrated to `createAttributePayload` and `diffAttributePayloads`
    get deepDiffer() {
        return deepDiffer;
    },
    get deepFreezeAndThrowOnMutationInDev() {
        // Applicable only in DEV mode
        // return (...args) => console.log("deepFreezeAndThrowOnMutationInDev", args);
        return (...args) => {};
    },
    get flattenStyle() {
        return {};
    },
    get ReactFiberErrorDialog() {
        return {
            showErrorDialog(...args) {
                console.log("ReactFiberErrorDialog.showErrorDialog", args);
            },
        };
    },
    get legacySendAccessibilityEvent() {
        return {};
    },
    get RawEventEmitter() {
        return {
            emit(...args) {
                // console.log(args);
            },
        };
    },
    get CustomEvent() {
        return {};
    },
    get createAttributePayload() {
        return {};
    },
    get diffAttributePayloads() {
        return {};
    },
    get createPublicTextInstance() {
        return {};
    },
    get getNativeTagFromPublicInstance() {
        return {};
    },
    get getNodeFromPublicInstance() {
        return {};
    },
    get getInternalInstanceHandleFromPublicInstance() {
        return {};
    },
};
