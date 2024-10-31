import { useMemo, useRef } from "react";
// @ts-ignore
import getWasmModule from "./lib/wasm/xframes.mjs";
// @ts-ignore
import wasmDataPackage from "./lib/wasm/xframes.data";
// import { XFrameDemo } from "./lib";
import { GetWasmModule, ImGuiCol } from "./lib/wasm/wasm-app-types";
import { XFramesStyleForPatching } from "./lib/stylesheet/xframes-style";
import { TradingGuiDemo } from "./lib/components/TradingGuiDemo/TradingGuiDemo";
import { theme1, theme2 } from "./lib/stylesheet/themes";
import { XFrames } from "./lib/components/XFrames";

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
        [],
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
