import * as React from "react";
import { PropsWithChildren } from "react";
import { attachSubComponents } from "../src/lib/attachSubComponents";
import { components } from "../src/lib/components/XFrames/components";

export type MainComponentProps = PropsWithChildren;

export const MainComponent: React.ComponentType<MainComponentProps> = ({
    children,
}: MainComponentProps) => {
    return null;
};

export const XFrames = attachSubComponents("XFrames", MainComponent, components);
