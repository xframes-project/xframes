import React from "react";
import { useStore } from "../store";
import { CryptoCandlestickPlot } from "./CryptoCandlestickPlot";
import { ReactImgui } from "../../ReactImgui";

type Props = {};

export const CryptoCandlestickPlots = ({}: Props) => {
    const symbols = useStore((state) => state.symbols);

    return (
        <ReactImgui.Node
            style={{
                width: "100%",
                height: "100%",
                flexDirection: "column",
                gap: { row: 5 },
            }}
        >
            {symbols.map((sym) => (
                <CryptoCandlestickPlot key={sym} symbol={sym} />
            ))}
        </ReactImgui.Node>
    );
};
