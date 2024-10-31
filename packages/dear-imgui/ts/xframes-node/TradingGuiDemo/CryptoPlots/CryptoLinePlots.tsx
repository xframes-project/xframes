import React from "react";
import { useStore } from "../store";
import { CryptoLinePlot } from "./CryptoLinePlot";
import { ReactImgui } from "../../ReactImgui";

type Props = {};

export const CryptoLinePlots = ({}: Props) => {
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
                <CryptoLinePlot key={sym} symbol={sym} />
            ))}
        </ReactImgui.Node>
    );
};
