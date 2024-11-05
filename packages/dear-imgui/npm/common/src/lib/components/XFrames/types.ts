import { FunctionComponent, JSXElementConstructor, ReactElement, SyntheticEvent } from "react";
import { StyleRules } from "src/lib/stylesheet/stylesheet";
import { YogaStyle } from "src/lib/stylesheet/yoga-style";
import { BaseDrawStyle } from "src/lib/stylesheet/base-draw-style";
import { TreeViewItem } from "./TreeView";
import { ImPlotMarker, ImPlotScale, ImVec2 } from "src/lib/types";

export type ModuleEventHandlers = {
    onTextChange: (id: number, value: string) => void;
    onComboChange: (id: number, value: number) => void;
    onNumericValueChange: (id: number, value: number) => void;
    onMultiValueChange: (id: number, values: Primitive[]) => void;
    onBooleanValueChange: (id: number, value: boolean) => void;
    onClick: (id: number) => void;
    onInit?: () => void;
};

export type WasmDeps = {
    eventHandlers: ModuleEventHandlers;
};

export type Primitive = string | number | boolean;

export type SliderTypes = "default" | "angle";

export type PlotCandlestickDataItem = {
    date: number;
    open: number;
    close: number;
    low: number;
    high: number;
};

export type FontDef = {
    name: string;
    sizes: number[];
};

export type NodeStyle = YogaStyle & BaseDrawStyle;
export type WidgetStyle = StyleRules & YogaStyle & BaseDrawStyle;

export type NodeStyleProps = {
    style?: NodeStyle;
    hoverStyle?: NodeStyle;
    activeStyle?: NodeStyle;
    disabledStyle?: NodeStyle;
};

export type WidgetStyleProps = {
    style?: WidgetStyle;
    hoverStyle?: WidgetStyle;
    activeStyle?: WidgetStyle;
    disabledStyle?: WidgetStyle;
};

export type TabItemChangeEvent = SyntheticEvent<WidgetReactElement<"TabItem">, { value: boolean }>;

export type InputTextChangeEvent = SyntheticEvent<
    WidgetReactElement<"InputText">,
    { value: string }
>;

export type ComboChangeEvent = SyntheticEvent<WidgetReactElement<"Combo">, { value: number }>;

export type SliderChangeEvent = SyntheticEvent<WidgetReactElement<"Slider">, { value: number }>;

export type MultiSliderChangeEvent<T extends Primitive = Primitive> = SyntheticEvent<
    WidgetReactElement<"MultiSlider">,
    { values: T[] }
>;

export type CheckboxChangeEvent = SyntheticEvent<
    WidgetReactElement<"Checkbox">,
    { value: boolean }
>;

export type WidgetPropsMap = {
    BulletText: WidgetStyleProps & {
        // todo: What about `fmt` ?
        text: string;
    };
    Button: WidgetStyleProps & {
        onClick?: () => void;
        label?: string;
        size?: ImVec2;
    };
    Checkbox: WidgetStyleProps & {
        defaultChecked?: boolean;
        label?: string;
        onChange?: (event: CheckboxChangeEvent) => void;
    };
    Child: WidgetStyleProps;
    ClippedMultiLineTextRenderer: WidgetStyleProps;
    CollapsingHeader: WidgetStyleProps & {
        label?: string;
    };
    Combo: WidgetStyleProps & {
        placeholder?: string;
        options?: string[];
        optionsList?: string;
        initialSelectedIndex?: number;
        onChange?: (event: ComboChangeEvent) => void;
    };
    DisabledText: WidgetStyleProps & {
        text: string;
    };
    DIWindow: WidgetStyleProps & { title: string; width?: number; height?: number };
    Group: WidgetStyleProps;
    HelpMarker: WidgetStyleProps & {
        text: string;
    };
    Image: WidgetStyleProps & { url: string; width?: number; height?: number };
    Indent: WidgetStyleProps;
    InputText: WidgetStyleProps & {
        defaultValue?: string;
        hint?: string;
        onChange?: (event: InputTextChangeEvent) => void;
    };
    ItemTooltip: WidgetStyleProps;
    MapView: WidgetStyleProps;
    MultiSlider: WidgetStyleProps & {
        numValues: 2 | 3 | 4;
        defaultValues?: number[];
        min?: number;
        max?: number;
        decimalDigits?: number;
        onChange?: (event: MultiSliderChangeEvent<number>) => void;
    };
    PlotCandlestick: WidgetStyleProps & {
        bullColor?: string;
        bearColor?: string;
        axisAutoFit?: boolean;
    };
    PlotLine: WidgetStyleProps & {
        xAxisDecimalDigits?: number;
        yAxisDecimalDigits?: number;
        markerStyle?: ImPlotMarker;
        xAxisScale?: ImPlotScale;
        yAxisScale?: ImPlotScale;
        axisAutoFit?: boolean;
    };
    Separator: WidgetStyleProps;
    SeparatorText: WidgetStyleProps & {
        label: string;
    };
    Slider: WidgetStyleProps & {
        sliderType?: SliderTypes;
        defaultValue?: number;
        min?: number;
        max?: number;
        onChange?: (event: SliderChangeEvent) => void;
    };
    TabBar: WidgetStyleProps;
    TabItem: WidgetStyleProps & {
        label: string;
        onOpenChange?: (event: TabItemChangeEvent) => void;
    };
    Table: WidgetStyleProps & {
        columns: { heading: string; fieldId?: string }[];
        initialData?: string;
        clipRows?: number;
    };
    TextWrap: WidgetStyleProps & { width: number };
    TreeNode: WidgetStyleProps & {
        itemId: string;
        onClick?: () => void;
        leaf?: boolean;
        open?: boolean;
        defaultOpen?: boolean;
        selected?: boolean;
        defaultSelected?: boolean;
        selectable?: boolean;
        label?: string;
    };
    TreeView: WidgetStyleProps & {
        items: TreeViewItem[];
        defaultSelectedItemIds?: string[];
        selectedItemIds?: string[];
        allowMultipleSelection?: boolean;
        onToggleItemSelection?: (itemId: string, selected: boolean) => void;
    };
    UnformattedText: WidgetStyleProps & {
        text: string;
    };
};

export type WidgetKeys = keyof WidgetPropsMap;

export type WidgetsRequiringId =
    | "Button"
    | "Checkbox"
    | "ClippedMultiLineTextRenderer"
    | "CollapsingHeader"
    | "Combo"
    | "Image"
    | "InputText"
    | "MapView"
    | "MultiSlider"
    | "PlotLine"
    | "PlotCandlestick"
    | "Slider"
    | "Table"
    | "TabBar"
    | "TabItem"
    | "TreeNode";

export type WidgetReactNode =
    | WidgetReactElementsFlat
    | null
    | undefined
    | false
    | Iterable<WidgetReactNode>;

export type WidgetReactElement<K extends WidgetKeys> = ReactElement<
    WidgetPropsMap[K],
    JSXElementConstructor<WidgetPropsMap[K]>
>;
type WidgetReactElements = {
    [K in WidgetKeys]: WidgetReactElement<K>;
};
type WidgetReactElementsFlat = WidgetReactElements[keyof WidgetReactElements];

export type ReactElementWidget<
    K extends WidgetKeys,
    P extends WidgetPropsMapFlat = WidgetPropsMapFlat,
> = K extends WidgetsRequiringId
    ? { [L in keyof Omit<P, "children">]: P[L] } & {
          id: string;
          root?: boolean;
          children?: WidgetReactNode;
      } & { onChange?: any; onClick?: any }
    : { [L in keyof Omit<P, "children">]: P[L] } & {
          root?: boolean;
          children?: WidgetReactNode;
      };

export type ReactElementWidgets = {
    [K in WidgetKeys]: ReactElementWidget<K, WidgetPropsMap[K]>;
};

export type ReactElementWidgetsFlat = ReactElementWidgets[keyof ReactElementWidgets];

export type NodeProps = {
    root?: boolean;
    cull?: boolean;
    trackMouseClickEvents?: boolean;
    children?: WidgetReactNode;
    onClick?: () => void;
} & NodeStyleProps;

export type YogaNode = NodeProps & { id?: string };

export type XFramesWidget<
    K extends WidgetKeys,
    P extends WidgetPropsMapFlat = WidgetPropsMapFlat,
> = K extends WidgetsRequiringId
    ? { [L in keyof Omit<P, "children">]: P[L] } & {
          id: string;
          type: K;
          children?: XFramesWidget<WidgetKeys>[];
      }
    : { [L in keyof Omit<P, "children">]: P[L] } & {
          type: K;
          children?: XFramesWidget<WidgetKeys>[];
      };

type XFramesWidgets = {
    [K in WidgetKeys]: XFramesWidget<K, WidgetPropsMap[K]>;
};

export type XFramesWidgetsFlat = XFramesWidgets[keyof XFramesWidgets];

export type WidgetPropsMapFlat = WidgetPropsMap[keyof WidgetPropsMap];

export type PropsWithChildren<P = unknown> = P & {
    children?: WidgetReactNode;
};

export type WidgetFunctionComponent<P = {}> = FunctionComponent<P> & {
    (props: P): WidgetReactElementsFlat;
};

export type JSXWidgetNode<
    K extends WidgetKeys,
    P extends WidgetPropsMapFlat = WidgetPropsMapFlat,
> = K extends WidgetsRequiringId
    ? {
          type: "widget";
          props: {
              id: string;
              type: K;
          } & P;
          children?: JSXWidgetNodesFlat[];
      }
    : {
          type: "widget";
          props: {
              type: K;
          } & P;
          children?: JSXWidgetNodesFlat[];
      };

type JSXWidgetNodes = {
    [K in WidgetKeys]: JSXWidgetNode<K, WidgetPropsMap[K]>;
};

export type JSXWidgetNodesFlat = JSXWidgetNodes[keyof JSXWidgetNodes];
