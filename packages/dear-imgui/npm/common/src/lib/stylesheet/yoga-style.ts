export type Edge =
    | "left"
    | "top"
    | "right"
    | "bottom"
    | "start"
    | "end"
    | "horizontal"
    | "vertical"
    | "all";

export type Gutter = "column" | "row" | "all";

export type YogaStyle = {
    direction?: "inherit" | "ltr" | "rtl";
    flexDirection?: "column" | "column-reverse" | "row" | "row-reverse";
    justifyContent?:
        | "flex-start"
        | "center"
        | "flex-end"
        | "space-between"
        | "space-around"
        | "space-evenly";
    alignContent?:
        | "auto"
        | "flex-start"
        | "center"
        | "flex-end"
        | "stretch"
        | "space-between"
        | "space-around"
        | "space-evenly";
    alignItems?: "auto" | "flex-start" | "center" | "flex-end" | "stretch" | "baseline";
    alignSelf?: "auto" | "flex-start" | "center" | "flex-end" | "stretch" | "baseline";
    positionType?: "static" | "relative" | "absolute";
    flexWrap?: "no-wrap" | "wrap" | "wrap-reverse";
    overflow?: "visible" | "hidden" | "scroll";
    display?: "flex" | "none";
    flex?: number;
    flexGrow?: number;
    flexShrink?: number;
    flexBasis?: number;
    flexBasisPercent?: number;
    position?: {
        [edge in Edge]?: number;
    };
    margin?: {
        [edge in Edge]?: number;
    };
    padding?: {
        [edge in Edge]?: number;
    };
    gap?: {
        [gutter in Gutter]?: number;
    };
    aspectRatio?: number;
    width?: number | string;
    minWidth?: number | string;
    maxWidth?: number | string;
    height?: number | string;
    minHeight?: number | string;
    maxHeight?: number | string;
};
