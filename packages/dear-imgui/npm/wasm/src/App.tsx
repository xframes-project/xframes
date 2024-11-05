import { useMemo, useRef } from "react";
// @ts-ignore
import getWasmModule from "./lib/xframes.mjs";
// @ts-ignore
import wasmDataPackage from "./lib/xframes.data";

import { XFramesStyleForPatching } from "@xframes/common";
import { TradingGuiDemo } from "./components/TradingGuiDemo/TradingGuiDemo";
import { XFrames } from "./lib";
import { GetWasmModule } from "./lib/wasm-app-types";
import { theme2 } from "./themes";

import "./App.css";

function App() {
  const containerRef = useRef<HTMLDivElement>(null);

  const fontDefs = useMemo(
    () => [
      { name: "roboto-regular", sizes: [16, 18, 20, 24, 28, 32, 36, 48] },
      { name: "roboto-bold", sizes: [16, 18, 20, 24, 28, 32, 36, 48] },
      // { name: "roboto-light", sizes: [12, 14, 16, 18, 20, 24, 28, 32, 36, 48] },
      { name: "roboto-mono-regular", sizes: [14, 16] },
    ],
    []
  );

  const defaultFont = useMemo(() => ({ name: "roboto-regular", size: 16 }), []);

  const styleOverrides: XFramesStyleForPatching = useMemo(() => theme2, []);

  return (
    <div id="app" ref={containerRef}>
      <XFrames
        getWasmModule={getWasmModule as GetWasmModule}
        wasmDataPackage={wasmDataPackage as string}
        containerRef={containerRef}
        fontDefs={fontDefs}
        defaultFont={defaultFont}
        styleOverrides={styleOverrides}
      >
        {/* <XFrameDemo /> */}
        <TradingGuiDemo />
      </XFrames>
    </div>
  );
}

export default App;
