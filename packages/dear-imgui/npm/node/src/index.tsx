import * as React from "react";
import { App } from "./App";
import { theme2 } from "./themes";
import { render } from "./lib/render";

const fontDefs: any = {
  defs: [
    { name: "roboto-regular", sizes: [16, 18, 20, 24, 28, 32, 36, 48] },
    // { name: "roboto-bold", sizes: [16, 18, 20, 24, 28, 32, 36, 48] },
    // { name: "roboto-light", sizes: [12, 14, 16, 18, 20, 24, 28, 32, 36, 48] },
    // { name: "roboto-mono-regular", sizes: [14, 16] },
  ]
    .map(({ name, sizes }) => sizes.map((size) => ({ name, size })))
    .flat(),
};

const assetsBasePath = "../../assets";

render(App, assetsBasePath, fontDefs, theme2);
