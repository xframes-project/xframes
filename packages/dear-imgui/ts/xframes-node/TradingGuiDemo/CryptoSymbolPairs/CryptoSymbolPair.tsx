import React, { useMemo } from "react";
import { ReactImgui } from "../../ReactImgui";
import RWStyleSheet from "../../../src/lib/stylesheet/stylesheet";

type Props = {
    symbol: string;
};

export const CryptoSymbolPair = ({ symbol }: Props) => {
    const styleSheet = useMemo(
        () =>
            RWStyleSheet.create({
                symbolLogoPair: {
                    flexDirection: "row",
                },
                symbolLogoLeft: {
                    width: 24,
                    height: 24,
                },
                symbolLogoRight: {
                    width: 24,
                    height: 24,
                    position: { left: -5 }, // todo: doesn't this need absolute positioning?
                },
            }),
        [],
    );

    return (
        <ReactImgui.Node style={styleSheet.symbolLogoPair}>
            <ReactImgui.Image url="bitcoin-btc-logo_gqud0f.png" style={styleSheet.symbolLogoLeft} />

            <ReactImgui.Image
                url="77-771550_dollar-currency-money-finance-usd-us-dollar-dollar_apzzg2.png"
                style={styleSheet.symbolLogoRight}
            />
        </ReactImgui.Node>
    );
};
