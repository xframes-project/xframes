import { useMemo, useRef } from "react";
// @ts-ignore
import getWasmModule from "./lib/wasm/xframes.mjs";
// @ts-ignore
import wasmDataPackage from "./lib/wasm/xframes.data";
// import { ImGuiDemo } from "./lib";
import { GetWasmModule, ImGuiCol } from "./lib/wasm/wasm-app-types";
import { ImGuiStyleForPatching } from "./lib/stylesheet/imgui-style";
import { TradingGuiDemo } from "./lib/components/TradingGuiDemo/TradingGuiDemo";
import { theme1, theme2 } from "./lib/stylesheet/themes";
import { ReactImgui } from "./lib/components/ReactImgui";

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

    const styleOverrides: ImGuiStyleForPatching = useMemo(() => theme2, []);

    return (
        <div id="app" ref={containerRef}>
            <ReactImgui
                getWasmModule={getWasmModule as GetWasmModule}
                wasmDataPackage={wasmDataPackage as string}
                containerRef={containerRef}
                fontDefs={fontDefs}
                defaultFont={defaultFont}
                styleOverrides={styleOverrides}
            >
                {/* <ImGuiDemo /> */}
                <TradingGuiDemo />
            </ReactImgui>
        </div>
    );
}

export default App;
