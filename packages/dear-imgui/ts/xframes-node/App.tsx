import * as React from "react";
import { XFrames } from "./XFrames";
import { TradingGuiDemo } from "./TradingGuiDemo/TradingGuiDemo";

export const App = () => {
    return (
        <XFrames.Node root style={{ width: "100%", height: "100%" }}>
            <TradingGuiDemo />
        </XFrames.Node>
    );
};
