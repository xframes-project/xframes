import { render } from "./render";
import { XFrames } from "./XFrames";

const xframes = require("./xframes.node");

export const captureScreenshot = (path: string): Promise<void> =>
  new Promise((resolve, reject) => {
    xframes.captureScreenshot(path, (error: string | null) => {
      if (error) {
        reject(new Error(error));
        return;
      }

      resolve();
    });
  });

export { XFrames, render };
