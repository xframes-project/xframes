import { resolve } from "path";
import * as React from "react";
import { theme2 } from "./themes";
import { render, XFrames } from "@xframes/node";

const fontDefs: any = {
  defs: [{ name: "roboto-regular", sizes: [16, 18, 20, 24] }]
    .map(({ name, sizes }) => sizes.map((size) => ({ name, size })))
    .flat(),
};

const assetsBasePath = resolve("./assets");

const App = () => (
  <XFrames.Node root style={{ height: "100%" }}>
    <XFrames.UnformattedText text="Hello, world" />
  </XFrames.Node>
);

render(App, assetsBasePath, fontDefs, theme2);
