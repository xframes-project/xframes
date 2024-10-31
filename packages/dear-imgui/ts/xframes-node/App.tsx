import * as React from "react";
import { ReactImgui } from "./ReactImgui";
import { TradingGuiDemo } from "./TradingGuiDemo/TradingGuiDemo";

export const App = () => {
    return (
        <ReactImgui.Node root style={{ width: "100%", height: "100%" }}>
            <TradingGuiDemo />
        </ReactImgui.Node>
    );
};
