import { ImGuiCol, ImGuiStyleVar, ImVec2 } from "../types";
import { BaseDrawStyle } from "./base-draw-style";
import { StyleColValue } from "./types";
import { YogaStyle } from "./yoga-style";

export type StyleRules = {
    align?: "left" | "right";
    font?: {
        name: string;
        size: number;
    };
    colors?: { [k in ImGuiCol]?: StyleColValue };
    vars?: {
        [ImGuiStyleVar.Alpha]?: number;
        [ImGuiStyleVar.DisabledAlpha]?: number;
        [ImGuiStyleVar.WindowPadding]?: ImVec2;
        [ImGuiStyleVar.WindowRounding]?: number;
        [ImGuiStyleVar.WindowBorderSize]?: number;
        [ImGuiStyleVar.WindowMinSize]?: ImVec2;
        [ImGuiStyleVar.WindowTitleAlign]?: ImVec2;
        [ImGuiStyleVar.ChildRounding]?: number;
        [ImGuiStyleVar.ChildBorderSize]?: number;
        [ImGuiStyleVar.PopupRounding]?: number;
        [ImGuiStyleVar.PopupBorderSize]?: number;
        [ImGuiStyleVar.FramePadding]?: ImVec2;
        [ImGuiStyleVar.FrameRounding]?: number;
        [ImGuiStyleVar.FrameBorderSize]?: number;
        [ImGuiStyleVar.ItemSpacing]?: ImVec2;
        [ImGuiStyleVar.ItemInnerSpacing]?: ImVec2;
        [ImGuiStyleVar.IndentSpacing]?: number;
        [ImGuiStyleVar.CellPadding]?: ImVec2;
        [ImGuiStyleVar.ScrollbarSize]?: number;
        [ImGuiStyleVar.ScrollbarRounding]?: number;
        [ImGuiStyleVar.GrabMinSize]?: number;
        [ImGuiStyleVar.GrabRounding]?: number;
        [ImGuiStyleVar.TabRounding]?: number;
        [ImGuiStyleVar.TabBorderSize]?: number;
        [ImGuiStyleVar.TabBarBorderSize]?: number;
        [ImGuiStyleVar.TableAngledHeadersAngle]?: number;
        [ImGuiStyleVar.TableAngledHeadersTextAlign]?: ImVec2;
        [ImGuiStyleVar.ButtonTextAlign]?: ImVec2;
        [ImGuiStyleVar.SelectableTextAlign]?: ImVec2;
        [ImGuiStyleVar.SeparatorTextBorderSize]?: number;
        [ImGuiStyleVar.SeparatorTextAlign]?: ImVec2;
        [ImGuiStyleVar.SeparatorTextPadding]?: ImVec2;
    };
};

export type RWStyleSheet = {
    [k: string]: StyleRules & YogaStyle & BaseDrawStyle;
};

const RWStyleSheet = {
    create: (styleSheet: RWStyleSheet): RWStyleSheet => styleSheet,
};

export default RWStyleSheet;
