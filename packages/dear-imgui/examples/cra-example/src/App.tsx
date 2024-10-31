import React, { useEffect, useMemo, useRef, useState } from "react";
import {
  XFrames,
  XFramesDemo,
  ImGuiCol,
  XFramesStyleForPatching,
} from "@xframes/wasm";
// @ts-ignore
import getWasmModule from "@xframes/wasm/dist/xframes.mjs";
// @ts-ignore
import wasmDataPackage from "@xframes/wasm/dist/xframes.data";

import "./App.css";

export const themeColors = {
  black: "#1a1a1a",
  darkGrey: "#5a5a5a",
  grey: "#9a9a9a",
  lightGrey: "#bebebe",
  veryLightGrey: "#e5e5e5",
  superLightGrey: "#f7f7f7",
  white: "#ffffff",
  hero: "#ff6e59",
  hoverHero: "#ff4a30",
};

function App() {
  const containerRef = useRef<HTMLDivElement>(null);

  const fontDefs = useMemo(
    () => [
      { name: "roboto-regular", sizes: [12, 14, 16, 18, 24] },
      { name: "roboto-bold", sizes: [12, 14, 16, 18, 24] },
      { name: "roboto-light", sizes: [12, 14, 16, 18, 24] },
      { name: "roboto-mono-regular", sizes: [14] },
    ],
    []
  );

  const defaultFont = useMemo(() => ({ name: "roboto-regular", size: 16 }), []);

  const styleOverrides: XFramesStyleForPatching = useMemo(
    () => ({
      // frameBorderSize: 1,
      // windowPadding: [20, 20],
      colors: {
        [ImGuiCol.Text]: [themeColors.black, 1],
        [ImGuiCol.TextDisabled]: [themeColors.darkGrey, 1],
        [ImGuiCol.WindowBg]: [themeColors.white, 1],
        [ImGuiCol.ChildBg]: [themeColors.white, 1],
        [ImGuiCol.PopupBg]: [themeColors.white, 1],
        [ImGuiCol.Border]: [themeColors.darkGrey, 1],
        [ImGuiCol.BorderShadow]: [themeColors.black, 1],
        [ImGuiCol.FrameBg]: [themeColors.veryLightGrey, 1],
        [ImGuiCol.FrameBgHovered]: [themeColors.lightGrey, 1],
        [ImGuiCol.FrameBgActive]: [themeColors.grey, 1],
        [ImGuiCol.TitleBg]: [themeColors.lightGrey, 1],
        [ImGuiCol.TitleBgActive]: [themeColors.grey, 1],
        [ImGuiCol.TitleBgCollapsed]: [themeColors.veryLightGrey, 1],
        [ImGuiCol.MenuBarBg]: [themeColors.grey, 1],
        [ImGuiCol.ScrollbarBg]: [themeColors.veryLightGrey, 1],
        [ImGuiCol.ScrollbarGrab]: [themeColors.grey, 1],
        [ImGuiCol.ScrollbarGrabHovered]: [themeColors.darkGrey, 1],
        [ImGuiCol.ScrollbarGrabActive]: [themeColors.black, 1],
        [ImGuiCol.CheckMark]: [themeColors.black, 1],
        [ImGuiCol.SliderGrab]: [themeColors.grey, 1],
        [ImGuiCol.SliderGrabActive]: [themeColors.darkGrey, 1],
        [ImGuiCol.Button]: [themeColors.lightGrey, 1],
        [ImGuiCol.ButtonHovered]: [themeColors.grey, 1],
        [ImGuiCol.ButtonActive]: [themeColors.darkGrey, 1],
        [ImGuiCol.Header]: [themeColors.veryLightGrey, 1],
        [ImGuiCol.HeaderHovered]: [themeColors.lightGrey, 1],
        [ImGuiCol.HeaderActive]: [themeColors.grey, 1],
        [ImGuiCol.Separator]: [themeColors.superLightGrey, 1],
        [ImGuiCol.SeparatorHovered]: [themeColors.veryLightGrey, 1],
        [ImGuiCol.SeparatorActive]: [themeColors.lightGrey, 1],
        [ImGuiCol.ResizeGrip]: [themeColors.veryLightGrey, 1],
        [ImGuiCol.ResizeGripHovered]: [themeColors.lightGrey, 1],
        [ImGuiCol.ResizeGripActive]: [themeColors.grey, 1],
        [ImGuiCol.Tab]: [themeColors.veryLightGrey, 1],
        [ImGuiCol.TabHovered]: [themeColors.lightGrey, 1],
        [ImGuiCol.TabActive]: [themeColors.grey, 1],
        [ImGuiCol.TabUnfocused]: [themeColors.veryLightGrey, 1],
        [ImGuiCol.TabUnfocusedActive]: [themeColors.lightGrey, 1],
        [ImGuiCol.PlotLines]: [themeColors.grey, 1],
        [ImGuiCol.PlotLinesHovered]: [themeColors.darkGrey, 1],
        [ImGuiCol.PlotHistogram]: [themeColors.grey, 1],
        [ImGuiCol.PlotHistogramHovered]: [themeColors.darkGrey, 1],
        [ImGuiCol.TableHeaderBg]: [themeColors.grey, 1],
        [ImGuiCol.TableBorderStrong]: [themeColors.darkGrey, 1],
        [ImGuiCol.TableBorderLight]: [themeColors.lightGrey, 1],
        [ImGuiCol.TableRowBg]: [themeColors.veryLightGrey, 1],
        [ImGuiCol.TableRowBgAlt]: [themeColors.white, 1],
        [ImGuiCol.TextSelectedBg]: [themeColors.grey, 1],
        [ImGuiCol.DragDropTarget]: [themeColors.grey, 1],
        [ImGuiCol.NavHighlight]: [themeColors.grey, 1],
        [ImGuiCol.NavWindowingHighlight]: [themeColors.grey, 1],
        [ImGuiCol.NavWindowingDimBg]: [themeColors.grey, 1],
        [ImGuiCol.ModalWindowDimBg]: [themeColors.grey, 1],
      },
    }),
    []
  );

  return (
    <div id="app" ref={containerRef}>
      <XFrames
        wasmDataPackage={wasmDataPackage}
        getWasmModule={getWasmModule}
        containerRef={containerRef}
        fontDefs={fontDefs}
        defaultFont={defaultFont}
        styleOverrides={styleOverrides}
      >
        <XFramesDemo />
      </XFrames>
    </div>
  );
}

export default App;
