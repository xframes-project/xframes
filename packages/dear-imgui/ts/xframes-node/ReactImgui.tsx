import * as React from "react";
import { PropsWithChildren } from "react";
import ReactNativePrivateInterface from "../src/lib/react-native/ReactNativePrivateInterface";
import { FontDef, ImGuiStyleForPatching, useDearImguiWasm } from "../src/lib";
import { useDearImguiFonts } from "../src/lib/hooks";
import { ReactNativeWrapper } from "./ReactNativeWrapper";
import { attachSubComponents } from "../src/lib/attachSubComponents";
import { components } from "../src/lib/components/XFrames/components";

export type MainComponentProps = PropsWithChildren & {
    fontDefs?: FontDef[];
    defaultFont?: { name: string; size: number };
    styleOverrides?: ImGuiStyleForPatching;
};

export const MainComponent: React.ComponentType<MainComponentProps> = ({
    children,
    fontDefs,
    defaultFont,
    styleOverrides,
}: MainComponentProps) => {
    const { eventHandlers } = useDearImguiWasm(ReactNativePrivateInterface);
    const fonts = useDearImguiFonts(fontDefs);

    return <ReactNativeWrapper nodeImgui>{children}</ReactNativeWrapper>;
};

export const XFrames = attachSubComponents("XFrames", MainComponent, components);
