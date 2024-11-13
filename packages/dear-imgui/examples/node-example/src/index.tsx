import * as React from "react";
import { resolve } from "path";
import { faIconMap } from "@xframes/common";
import { render, XFrames } from "@xframes/node";
import { theme2 } from "./themes";

const fontDefs: any = {
  defs: [{ name: "roboto-regular", sizes: [16, 18, 20, 24] }]
    .map(({ name, sizes }) => sizes.map((size) => ({ name, size })))
    .flat(),
};

const assetsBasePath = resolve("./assets");

const App = () => (
  <XFrames.Node root style={{ height: "100%" }}>
    <XFrames.UnformattedText text={`Hello, world ${faIconMap["arrow-down"]}`} />
  </XFrames.Node>
);

render(App, assetsBasePath, fontDefs, theme2);
