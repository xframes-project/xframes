import React, { useMemo } from "react";
import { ImGuiCol, RWStyleSheet } from "@xframes/common";
import { CryptoSymbolPair } from "../CryptoSymbolPairs/CryptoSymbolPair";
import { XFrames } from "src/lib/XFrames";

type Props = {
  symbol: string;
};

export const CryptoSymbolBlock = ({ symbol }: Props) => {
  const price = "40,730.00";
  const currency = "USD";
  const trend = "(+2.50%)";

  const styleSheet = useMemo(
    () =>
      RWStyleSheet.create({
        block: {
          flexDirection: "row",
          padding: {
            // horizontal: 2,
            vertical: 8,
          },
          gap: { column: 5 },
          // borderColor: "#fff",
          // borderThickness: 1,
        },
        symbolLogos: {
          width: 60,
          height: 40,
          alignItems: "center",
          justifyContent: "center",
        },
        info: {
          gap: { row: 5 },
        },
        priceRow: {
          flexDirection: "row",
          gap: { column: 2 },
        },
        symbol: {
          font: { name: "roboto-bold", size: 16 },
          colors: { [ImGuiCol.Text]: "#A9B5C0" },
        },
        price: {
          font: { name: "roboto-bold", size: 20 },
          colors: { [ImGuiCol.Text]: "#D9D9D9" },
        },
        currency: {
          font: { name: "roboto-bold", size: 20 },
          colors: { [ImGuiCol.Text]: "#A9B5C0" },
        },
        trend: {
          font: { name: "roboto-bold", size: 20 },
          colors: { [ImGuiCol.Text]: "#239465" },
        },
      }),
    []
  );

  return (
    <XFrames.Node style={styleSheet.block}>
      <XFrames.Node style={styleSheet.symbolLogos}>
        <CryptoSymbolPair symbol={symbol} />
      </XFrames.Node>
      <XFrames.Node style={styleSheet.info}>
        <XFrames.UnformattedText style={styleSheet.symbol} text={symbol} />
        <XFrames.Node style={styleSheet.priceRow}>
          <XFrames.UnformattedText style={styleSheet.price} text={price} />
          <XFrames.UnformattedText
            style={styleSheet.currency}
            text={currency}
          />
          <XFrames.UnformattedText style={styleSheet.trend} text={trend} />
        </XFrames.Node>
      </XFrames.Node>
    </XFrames.Node>
  );
};
