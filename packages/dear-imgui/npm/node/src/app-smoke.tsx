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
  process.env.XFRAMES_APP_SCREENSHOT_PATH ?? "./build/app-smoke.png",
);

render(App, "../../assets", fontDefs, theme2);

setTimeout(() => {
  captureScreenshot(outputPath)
    .then(() => {
      console.log(`App screenshot written to ${outputPath}`);
      process.exit(0);
    })
    .catch((error: unknown) => {
      console.error(error);
      process.exit(1);
    });
}, 2500);
