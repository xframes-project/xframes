import * as React from "react";
import { PropsWithChildren } from "react";
import { attachSubComponents, components } from "@xframes/common";

export type MainComponentProps = PropsWithChildren;

export const MainComponent: React.ComponentType<MainComponentProps> = ({
  children,
}: MainComponentProps) => {
  return null;
};

export const XFrames = attachSubComponents(
  "XFrames",
  MainComponent,
  components
);
