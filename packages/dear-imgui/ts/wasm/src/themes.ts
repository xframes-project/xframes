import { XFramesStyleForPatching, ImGuiCol } from "@xframes/common";

export const theme1Colors = {
  black: "#1a1a1a",
  darkGrey: "#5a5a5a",
  grey: "#9a9a9a",
  lightGrey: "#bebebe",
  veryLightGrey: "#e5e5e5",
  superLightGrey: "#f7f7f7",
  white: "#fff",
  hero: "#ff6e59",
  hoverHero: "#ff4a30",
};

export const theme1: XFramesStyleForPatching = {
  // frameBorderSize: 1,
  // framePadding: [0, 0],
  colors: {
    [ImGuiCol.Text]: [theme1Colors.black, 1],
    [ImGuiCol.TextDisabled]: [theme1Colors.darkGrey, 1],
    [ImGuiCol.WindowBg]: [theme1Colors.white, 1],
    [ImGuiCol.ChildBg]: [theme1Colors.white, 1],
    [ImGuiCol.PopupBg]: [theme1Colors.white, 1],
    [ImGuiCol.Border]: [theme1Colors.darkGrey, 1],
    [ImGuiCol.BorderShadow]: [theme1Colors.black, 1],
    [ImGuiCol.FrameBg]: [theme1Colors.veryLightGrey, 1],
    [ImGuiCol.FrameBgHovered]: [theme1Colors.lightGrey, 1],
    [ImGuiCol.FrameBgActive]: [theme1Colors.grey, 1],
    [ImGuiCol.TitleBg]: [theme1Colors.lightGrey, 1],
    [ImGuiCol.TitleBgActive]: [theme1Colors.grey, 1],
    [ImGuiCol.TitleBgCollapsed]: [theme1Colors.veryLightGrey, 1],
    [ImGuiCol.MenuBarBg]: [theme1Colors.grey, 1],
    [ImGuiCol.ScrollbarBg]: [theme1Colors.veryLightGrey, 1],
    [ImGuiCol.ScrollbarGrab]: [theme1Colors.grey, 1],
    [ImGuiCol.ScrollbarGrabHovered]: [theme1Colors.darkGrey, 1],
    [ImGuiCol.ScrollbarGrabActive]: [theme1Colors.black, 1],
    [ImGuiCol.CheckMark]: [theme1Colors.black, 1],
    [ImGuiCol.SliderGrab]: [theme1Colors.grey, 1],
    [ImGuiCol.SliderGrabActive]: [theme1Colors.darkGrey, 1],
    [ImGuiCol.Button]: [theme1Colors.lightGrey, 1],
    [ImGuiCol.ButtonHovered]: [theme1Colors.grey, 1],
    [ImGuiCol.ButtonActive]: [theme1Colors.darkGrey, 1],
    [ImGuiCol.Header]: [theme1Colors.veryLightGrey, 1],
    [ImGuiCol.HeaderHovered]: [theme1Colors.lightGrey, 1],
    [ImGuiCol.HeaderActive]: [theme1Colors.grey, 1],
    [ImGuiCol.Separator]: [theme1Colors.black, 1],
    [ImGuiCol.SeparatorHovered]: [theme1Colors.veryLightGrey, 1],
    [ImGuiCol.SeparatorActive]: [theme1Colors.lightGrey, 1],
    [ImGuiCol.ResizeGrip]: [theme1Colors.veryLightGrey, 1],
    [ImGuiCol.ResizeGripHovered]: [theme1Colors.lightGrey, 1],
    [ImGuiCol.ResizeGripActive]: [theme1Colors.grey, 1],
    [ImGuiCol.Tab]: [theme1Colors.veryLightGrey, 1],
    [ImGuiCol.TabHovered]: [theme1Colors.lightGrey, 1],
    [ImGuiCol.TabActive]: [theme1Colors.grey, 1],
    [ImGuiCol.TabUnfocused]: [theme1Colors.veryLightGrey, 1],
    [ImGuiCol.TabUnfocusedActive]: [theme1Colors.lightGrey, 1],
    [ImGuiCol.PlotLines]: [theme1Colors.grey, 1],
    [ImGuiCol.PlotLinesHovered]: [theme1Colors.darkGrey, 1],
    [ImGuiCol.PlotHistogram]: [theme1Colors.grey, 1],
    [ImGuiCol.PlotHistogramHovered]: [theme1Colors.darkGrey, 1],
    [ImGuiCol.TableHeaderBg]: [theme1Colors.grey, 1],
    [ImGuiCol.TableBorderStrong]: [theme1Colors.darkGrey, 1],
    [ImGuiCol.TableBorderLight]: [theme1Colors.lightGrey, 1],
    [ImGuiCol.TableRowBg]: [theme1Colors.veryLightGrey, 1],
    [ImGuiCol.TableRowBgAlt]: [theme1Colors.white, 1],
    [ImGuiCol.TextSelectedBg]: [theme1Colors.grey, 1],
    [ImGuiCol.DragDropTarget]: [theme1Colors.grey, 1],
    [ImGuiCol.NavHighlight]: [theme1Colors.grey, 1],
    [ImGuiCol.NavWindowingHighlight]: [theme1Colors.grey, 1],
    [ImGuiCol.NavWindowingDimBg]: [theme1Colors.grey, 1],
    [ImGuiCol.ModalWindowDimBg]: [theme1Colors.grey, 1],
  },
};

export const theme2Colors = {
  darkestGrey: "#141f2c",
  darkerGrey: "#2a2e39",
  darkGrey: "#363b4a",
  lightGrey: "#5a5a5a",
  lighterGrey: "#7A818C",
  evenLighterGrey: "#8491a3",
  black: "#0A0B0D",
  green: "#75f986",
  red: "#ff0062",
  white: "#fff",
};

export const theme2: XFramesStyleForPatching = {
  // frameBorderSize: 1,
  // framePadding: [0, 0],
  colors: {
    [ImGuiCol.Text]: [theme2Colors.white, 1],
    [ImGuiCol.TextDisabled]: [theme2Colors.lighterGrey, 1],
    [ImGuiCol.WindowBg]: [theme2Colors.black, 1],
    [ImGuiCol.ChildBg]: [theme2Colors.black, 1],
    [ImGuiCol.PopupBg]: [theme2Colors.white, 1],
    [ImGuiCol.Border]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.BorderShadow]: [theme2Colors.darkestGrey, 1],
    [ImGuiCol.FrameBg]: [theme2Colors.black, 1],
    [ImGuiCol.FrameBgHovered]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.FrameBgActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TitleBg]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TitleBgActive]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.TitleBgCollapsed]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.MenuBarBg]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.ScrollbarBg]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.ScrollbarGrab]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.ScrollbarGrabHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.ScrollbarGrabActive]: [theme2Colors.darkestGrey, 1],
    [ImGuiCol.CheckMark]: [theme2Colors.darkestGrey, 1],
    [ImGuiCol.SliderGrab]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.SliderGrabActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.Button]: [theme2Colors.black, 1],
    [ImGuiCol.ButtonHovered]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.ButtonActive]: [theme2Colors.black, 1],
    [ImGuiCol.Header]: [theme2Colors.black, 1],
    [ImGuiCol.HeaderHovered]: [theme2Colors.black, 1],
    [ImGuiCol.HeaderActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.Separator]: [theme2Colors.darkestGrey, 1],
    [ImGuiCol.SeparatorHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.SeparatorActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.ResizeGrip]: [theme2Colors.black, 1],
    [ImGuiCol.ResizeGripHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.ResizeGripActive]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.Tab]: [theme2Colors.black, 1],
    [ImGuiCol.TabHovered]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.TabActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TabUnfocused]: [theme2Colors.black, 1],
    [ImGuiCol.TabUnfocusedActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.PlotLines]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.PlotLinesHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.PlotHistogram]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.PlotHistogramHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TableHeaderBg]: [theme2Colors.black, 1],
    [ImGuiCol.TableBorderStrong]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TableBorderLight]: [theme2Colors.darkerGrey, 1],
    [ImGuiCol.TableRowBg]: [theme2Colors.darkGrey, 1],
    [ImGuiCol.TableRowBgAlt]: [theme2Colors.darkerGrey, 1],
    [ImGuiCol.TextSelectedBg]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.DragDropTarget]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.NavHighlight]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.NavWindowingHighlight]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.NavWindowingDimBg]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.ModalWindowDimBg]: [theme2Colors.darkerGrey, 1], //
  },
};
