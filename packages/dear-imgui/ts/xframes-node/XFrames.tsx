import * as React from "react";
import { PropsWithChildren } from "react";
import ReactNativePrivateInterface from "../src/lib/react-native/ReactNativePrivateInterface";
import { FontDef, useXFramesWasm, XFramesStyleForPatching } from "../src/lib";
import { ReactNativeWrapper } from "./ReactNativeWrapper";
import { attachSubComponents } from "../src/lib/attachSubComponents";
import { components } from "../src/lib/components/XFrames/components";
import { useXFramesFonts } from "../src/lib/hooks";

export type MainComponentProps = PropsWithChildren & {
    fontDefs?: FontDef[];
    defaultFont?: { name: string; size: number };
    styleOverrides?: XFramesStyleForPatching;
};

export const MainComponent: React.ComponentType<MainComponentProps> = ({
    children,
    fontDefs,
    defaultFont,
    styleOverrides,
}: MainComponentProps) => {
    const { eventHandlers } = useXFramesWasm(ReactNativePrivateInterface);
    const fonts = useXFramesFonts(fontDefs);

    return <ReactNativeWrapper nodeImgui>{children}</ReactNativeWrapper>;
};

export const XFrames = attachSubComponents("XFrames", MainComponent, components);
