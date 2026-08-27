import { resolve } from "path";
import { App } from "./App";
import { captureScreenshot } from "./lib";
import { render } from "./lib/render";
import { theme2 } from "./themes";

const fontDefs = {
  defs: [
    { name: "roboto-regular", sizes: [16, 18, 20, 24, 28, 32, 36, 48] },
  ].flatMap(({ name, sizes }) => sizes.map((size) => ({ name, size }))),
};

const outputPath = resolve(
  process.env.XFRAMES_SCREENSHOT_PATH ?? "./build/screenshot-smoke.png",
);
const secondOutputPath = outputPath.replace(/\.png$/i, "-second.png");

render(App, "../../assets", fontDefs, theme2);

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
