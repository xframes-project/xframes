import NativeFabricUIManager from "./nativeFabricUiManager.ts";
import deepDiffer from "./deepDiffer.js";

const uiManager = new NativeFabricUIManager();

const commonAttributes = ["id", "style", "hoverStyle", "activeStyle", "disabledStyle"];

const attributesForElements = {
    "bullet-text": ["type", "text"],
    "di-button": ["onClick", "label", "size"],
    checkbox: ["defaultChecked", "label", "onChange"],
    child: ["defaultChecked", "label", "onChange"],
    "di-js-canvas": ["onScriptError"],
    "di-lua-canvas": ["onScriptError"],
    "di-janet-canvas": ["onScriptError"],
    "color-indicator": ["color", "shape"],
    "color-picker": ["defaultColor", "onChange"],
    "clipped-multi-line-text-renderer": [],
    "collapsing-header": ["label"],
    combo: ["placeholder", "options", "optionsList", "initialSelectedIndex", "onChange"],
    "disabled-text": ["text"],
    "di-window": ["title", "width", "number"],
    group: [],
    "help-marker": ["text"],
    "di-image": ["url", "width", "number"],
    indent: [],
    "input-text": ["defaultValue", "hint", "multiline", "password", "readOnly", "numericOnly", "onChange"],
    "item-tooltip": [],
    "map-view": ["onChange", "onPrefetchProgress", "tileUrlTemplate", "tileRequestHeaders", "attribution", "minZoom", "maxZoom", "cachePath"],
    "multi-slider": [
        "numValues",
        "label",
        "defaultValues",
        "min",
        "max",
        "decimalDigits",
        "onChange",
    ],
    "plot-bar": ["axisAutoFit", "dataPointsLimit", "xAxisLabel", "yAxisLabel", "showLegend", "legendLocation", "legendLabel"],
    "plot-histogram": ["bins", "axisAutoFit", "dataPointsLimit", "xAxisLabel", "yAxisLabel", "showLegend", "legendLocation", "legendLabel"],
    "plot-pie-chart": ["labelFormat", "angle0", "normalize", "showLegend", "legendLocation"],
    "plot-heatmap": ["axisAutoFit", "scaleMin", "scaleMax", "colormap", "xAxisLabel", "yAxisLabel", "showLegend", "legendLocation", "legendLabel"],
    "plot-scatter": ["axisAutoFit", "dataPointsLimit", "xAxisLabel", "yAxisLabel", "showLegend", "legendLocation", "legendLabel"],
    "plot-line": [
        "xAxisDecimalDigits",
        "yAxisDecimalDigits",
        "xAxisScale",
        "yAxisScale",
        "axisAutoFit",
        "markerStyle",
        "dataPointsLimit",
        "xAxisLabel",
        "yAxisLabel",
        "showLegend",
        "legendLocation",
        "legendLabel",
        "series",
    ],
    "plot-candlestick": ["axisAutoFit", "bullColor", "bearColor", "dataPointsLimit", "xAxisLabel", "yAxisLabel", "showLegend", "legendLocation", "legendLabel"],
    "progress-bar": ["fraction", "overlay"],
    separator: [],
    "separator-text": ["label"],
    slider: ["sliderType", "label", "defaultValue", "min", "max", "onChange"],
    "tab-bar": ["reorderable"],
    "tab-item": ["label", "closeable", "onChange"],
    "di-table": ["columns", "initialData", "clipRows", "filterable", "reorderable", "hideable", "contextMenuItems", "onSort", "onFilter", "onRowClick", "onItemAction"],
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
                onSort: { registrationName: "onSort" },
                onFilter: { registrationName: "onFilter" },
                onRowClick: { registrationName: "onRowClick" },
                onItemAction: { registrationName: "onItemAction" },
                onPrefetchProgress: { registrationName: "onPrefetchProgress" },
                onScriptError: { registrationName: "onScriptError" },
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
