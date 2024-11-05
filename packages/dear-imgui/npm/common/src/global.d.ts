import {
    ReactElementWidgets,
    ReactElementWidgetsFlat,
    YogaNode,
} from "./lib/components/XFrames/types";

export {};

declare global {
    namespace JSX {
        interface IntrinsicElements {
            "bullet-text": ReactElementWidgets["BulletText"];
            "di-button": ReactElementWidgets["Button"];
            checkbox: ReactElementWidgets["Checkbox"];
            child: ReactElementWidgets["Child"]; // Deprecated by `node`
            "clipped-multi-line-text-renderer": ReactElementWidgets["ClippedMultiLineTextRenderer"];
            "collapsing-header": ReactElementWidgets["CollapsingHeader"];
            combo: ReactElementWidgets["Combo"];
            "disabled-text": ReactElementWidgets["DisabledText"];
            "di-window": ReactElementWidgets["DIWindow"];
            group: ReactElementWidgets["Group"];
            "help-marker": ReactElementWidgets["HelpMarker"];
            "di-image": ReactElementWidgets["Image"];
            indent: ReactElementWidgets["Indent"];
            "input-text": ReactElementWidgets["InputText"];
            "item-tooltip": ReactElementWidgets["ItemTooltip"];
            "map-view": ReactElementWidgets["MapView"];
            "multi-slider": ReactElementWidgets["MultiSlider"];
            "plot-line": ReactElementWidgets["PlotLine"];
            "plot-candlestick": ReactElementWidgets["PlotCandlestick"];
            separator: ReactElementWidgets["Separator"];
            "separator-text": ReactElementWidgets["SeparatorText"];
            slider: ReactElementWidgets["Slider"];
            "tab-bar": ReactElementWidgets["TabBar"];
            "tab-item": ReactElementWidgets["TabItem"];
            "di-table": ReactElementWidgets["Table"];
            "text-wrap": ReactElementWidgets["TextWrap"];
            "tree-node": ReactElementWidgets["TreeNode"];
            "unformatted-text": ReactElementWidgets["UnformattedText"];
            node: YogaNode;
        }
    }
}
