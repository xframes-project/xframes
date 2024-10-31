import * as React from "react";
import { TradingGuiDemo } from "./TradingGuiDemo/TradingGuiDemo";
import { XFrames } from "./XFrames";

export const App = () => {
    return (
        <XFrames.Node root style={{ width: "100%", height: "100%" }}>
            <TradingGuiDemo />
        </XFrames.Node>
    );
};
