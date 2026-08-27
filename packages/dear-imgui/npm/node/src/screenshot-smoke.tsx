import * as React from "react";
import { resolve } from "path";
import { captureScreenshot, XFrames } from "./lib";
import { render } from "./lib/render";
import { theme2 } from "./themes";

const ScreenshotSmokeApp = () => (
  <XFrames.Node root style={{ width: "100%", height: "100%" }}>
    <XFrames.SeparatorText label="XFrames screenshot smoke test" />
    <XFrames.UnformattedText text="First and second frame captures" />
    <XFrames.Button label="Rendered by Dear ImGui" />
  </XFrames.Node>
);

const fontDefs = {
  defs: [
    { name: "roboto-regular", sizes: [16, 18, 20, 24, 28, 32, 36, 48] },
  ].flatMap(({ name, sizes }) => sizes.map((size) => ({ name, size }))),
};

const outputPath = resolve(
  process.env.XFRAMES_SCREENSHOT_PATH ?? "./build/screenshot-smoke.png",
);
const secondOutputPath = outputPath.replace(/\.png$/i, "-second.png");

render(ScreenshotSmokeApp, "../../assets", fontDefs, theme2);

setTimeout(() => {
  captureScreenshot(outputPath)
    .then(() => captureScreenshot(secondOutputPath))
    .then(() => {
      console.log(
        `Screenshots written to ${outputPath} and ${secondOutputPath}`,
      );
      process.exit(0);
    })
    .catch((error: unknown) => {
      console.error(error);
      process.exit(1);
    });
}, 1500);
