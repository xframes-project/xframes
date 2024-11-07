import * as React from "react";
import { theme2 } from "./themes";
import { render } from '@xframes/node/dist/lib/render';
import { XFrames } from '@xframes/node/dist/lib/XFrames';

const fontDefs: any = {
  defs: [
    { name: "roboto-regular", sizes: [16, 18, 20, 24] }
  ]
    .map(({ name, sizes }) => sizes.map((size) => ({ name, size })))
    .flat(),
};

const assetsBasePath = "../assets";

const App = () => <XFrames.Node root style={{height: "100%"}}>
    <XFrames.UnformattedText text="Hello, world"/>
</XFrames.Node>;

render(App, assetsBasePath, fontDefs, theme2);
