import { ImGuiCol, ImGuiDir, ImGuiHoveredFlags, ImVec2 } from "../wasm/wasm-app-types";
import { HEXA } from "./types";

export type StyleVarValue = number | ImVec2;

// This is a more or less accurate representation of ImGuiStyle
export type XFramesStyle = {
    alpha: number; // Global alpha applies to everything in Dear ImGui.
    disabledAlpha: number; // Additional alpha multiplier applied by BeginDisabled(). Multiply over current value of Alpha.
    windowPadding: ImVec2; // Padding within a window.
    windowRounding: number; // Radius of window corners rounding. Set to 0.0f to have rectangular windows. Large values tend to lead to variety of artifacts and are not recommended.
    windowBorderSize: number; // Thickness of border around windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    windowMinSize: ImVec2; // Minimum window size. This is a global setting. If you want to constrain individual windows, use SetNextWindowSizeConstraints().
    windowTitleAlign: ImVec2; // Alignment for title bar text. Defaults to (0.0f,0.5f) for left-aligned,vertically centered.
    windowMenuButtonPosition: ImGuiDir; // Side of the collapsing/docking button in the title bar (None/Left/Right). Defaults to ImGuiDir_Left.
    childRounding: number; // Radius of child window corners rounding. Set to 0.0f to have rectangular windows.
    childBorderSize: number; // Thickness of border around child windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    popupRounding: number; // Radius of popup window corners rounding. (Note that tooltip windows use WindowRounding)
    popupBorderSize: number; // Thickness of border around popup/tooltip windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    framePadding: ImVec2; // Padding within a framed rectangle (used by most widgets).
    frameRounding: number; // Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most widgets).
    frameBorderSize: 0 | 1; // Thickness of border around frames. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    itemSpacing: ImVec2; // Horizontal and vertical spacing between widgets/lines.
    itemInnerSpacing: ImVec2; // Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label).
    cellPadding: ImVec2; // Padding within a table cell. Cellpadding.x is locked for entire table. CellPadding.y may be altered between different rows.
    touchExtraPadding: ImVec2; // Expand reactive bounding box for touch-based system where touch position is not accurate enough. Unfortunately we don't sort widgets so priority on overlap will always be given to the first widget. So don't grow this too much!
    indentSpacing: number; // Horizontal indentation when e.g. entering a tree node. Generally == (FontSize + FramePadding.x*2).
    columnsMinSpacing: number; // Minimum horizontal spacing between two columns. Preferably > (FramePadding.x + 1).
    scrollbarSize: number; // Width of the vertical scrollbar, Height of the horizontal scrollbar.
    scrollbarRounding: number; // Radius of grab corners for scrollbar.
    grabMinSize: number; // Minimum width/height of a grab box for slider/scrollbar.
    grabRounding: number; // Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs.
    logSliderDeadzone: number; // The size in pixels of the dead-zone around zero on logarithmic sliders that cross zero.
    tabRounding: number; // Radius of upper corners of a tab. Set to 0.0f to have rectangular tabs.
    tabBorderSize: number; // Thickness of border around tabs.
    tabMinWidthForCloseButton: number; // Minimum width for close button to appear on an unselected tab when hovered. Set to 0.0f to always show when hovering, set to FLT_MAX to never show close button unless selected. ** todo: Figure out how to set FLT_MAX on the JS side - maybe using a string constant
    tabBarBorderSize: number; // Thickness of tab-bar separator, which takes on the tab active color to denote focus.
    tableAngledHeadersAngle: number; // Angle of angled headers (supported values range from -50.0f degrees to +50.0f degrees).
    tableAngledHeadersTextAlign: ImVec2; // Alignment of angled headers within the cell
    colorButtonPosition: ImGuiDir; // Side of the color button in the ColorEdit4 widget (left/right). Defaults to ImGuiDir_Right.
    buttonTextAlign: ImVec2; // Alignment of button text when button is larger than text. Defaults to (0.5f, 0.5f) (centered).
    selectableTextAlign: ImVec2; // Alignment of selectable text. Defaults to (0.0f, 0.0f) (top-left aligned). It's generally important to keep this left-aligned if you want to lay multiple items on a same line.
    separatorTextBorderSize: number; // Thickkness of border in SeparatorText()
    separatorTextAlign: ImVec2; // Alignment of text within the separator. Defaults to (0.0f, 0.5f) (left aligned, center).
    separatorTextPadding: ImVec2; // Horizontal offset of text from each edge of the separator + spacing on other axis. Generally small values. .y is recommended to be == FramePadding.y.
    displayWindowPadding: ImVec2; // Apply to regular windows: amount which we enforce to keep visible when moving near edges of your screen.
    displaySafeAreaPadding: ImVec2; // Apply to every windows, menus, popups, tooltips: amount where we avoid displaying contents. Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).
    mouseCursorScale: number; // Scale software rendered mouse cursor (when io.MouseDrawCursor is enabled). We apply per-monitor DPI scaling over this scale. May be removed later.
    antiAliasedLines: boolean; // Enable anti-aliased lines/borders. Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to ImDrawList).
    antiAliasedLinesUseTex: boolean; // Enable anti-aliased lines/borders using textures where possible. Require backend to render with bilinear filtering (NOT point/nearest filtering). Latched at the beginning of the frame (copied to ImDrawList).
    antiAliasedFill: boolean; // Enable anti-aliased edges around filled shapes (rounded rectangles, circles, etc.). Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to ImDrawList).
    curveTessellationTol: number; // Tessellation tolerance when using PathBezierCurveTo() without a specific number of segments. Decrease for highly tessellated curves (higher quality, more polygons), increase to reduce quality.
    circleTessellationMaxError: number; // Maximum error (in pixels) allowed when using AddCircle()/AddCircleFilled() or drawing rounded corner rectangles with no explicit segment count specified. Decrease for higher quality but more geometry.

    colors: HEXA[]; // 53 items exactly

    hoverStationaryDelay: number; // Delay for IsItemHovered(ImGuiHoveredFlags_Stationary). Time required to consider mouse stationary.
    hoverDelayShort: number; // Delay for IsItemHovered(ImGuiHoveredFlags_DelayShort). Usually used along with HoverStationaryDelay.
    hoverDelayNormal: number; // Delay for IsItemHovered(ImGuiHoveredFlags_DelayNormal).

    hoverFlagsForTooltipMouse: ImGuiHoveredFlags;
    hoverFlagsForTooltipNav: ImGuiHoveredFlags;
};

// This is what the WASM expects so that it can 'patch' the default style
export type XFramesStyleForPatching = Partial<Omit<XFramesStyle, "colors">> & {
    colors?: {
        [k in ImGuiCol]?: HEXA;
    };
};
