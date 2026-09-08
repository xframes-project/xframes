import { resolve } from "path";
import { App } from "./App";
import { captureScreenshot } from "./lib";
import { render } from "./lib/render";
import { theme2 } from "./themes";
import { ReactNativePrivateInterface } from "@xframes/common";

const native = require("./lib/xframes.node");

const fontDefs = {
  defs: [
    { name: "roboto-regular", sizes: [16, 18, 20, 24, 28, 32, 36, 48] },
  ].flatMap(({ name, sizes }) => sizes.map((size) => ({ name, size }))),
};

const outputPath = resolve(
  process.env.XFRAMES_APP_SCREENSHOT_PATH ?? "./build/app-smoke.png",
);

const unmount = render(App, "../../assets", fontDefs, theme2);
native.setDiagnosticsEnabled(true);

setTimeout(() => {
  captureScreenshot(outputPath)
    .then(async () => {
      console.log(`App screenshot written to ${outputPath}`);
      const populated = JSON.parse(native.getDiagnostics());
      if (populated.elementCount === 0) throw new Error("App rendered no native elements");
      await unmount();
      if (!ReactNativePrivateInterface.nativeFabricUIManager.getDiagnostics().subscriptionClosed)
        throw new Error("App wrapper did not dispose its bridge");
      const deadline = performance.now() + 10000;
      for (;;) {
        const frame = JSON.parse(native.getDiagnostics());
        if (frame.frame > populated.frame && frame.elementCount === 0 && frame.internalSubjectCount === 0) break;
        if (performance.now() > deadline) throw new Error("Populated App unmount did not reach an empty native frame");
        await new Promise(resolve => setTimeout(resolve, 20));
      }
      console.log("App populated unmount cleanup passed");
      process.exit(0);
    })
    .catch((error: unknown) => {
      console.error(error);
      process.exit(1);
    });
}, 2500);
