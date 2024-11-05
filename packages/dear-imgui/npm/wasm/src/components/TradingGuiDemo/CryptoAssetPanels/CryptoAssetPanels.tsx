import React, { useMemo } from "react";
import { RWStyleSheet } from "@xframes/common";
import { useStore } from "../store";
import { CryptoQuotePrice } from "../CryptoQuotePrice/CryptoQuotePrice";
import { XFrames } from "src/lib";

type Props = {};

export const CryptoAssetPanels = ({}: Props) => {
  const cryptoAssets = useStore((state) =>
    state.cryptoAssets.sort((a, b) => {
      if (a.symbol < b.symbol) {
        return -1;
      } else if (a.symbol > b.symbol) {
        return 1;
      }

      return 0;
    })
  );

  const styleSheet = useMemo(
    () =>
      RWStyleSheet.create({
        mainWrapperNode: {
          width: "100%",
          height: "auto",
          flexDirection: "row",
          flexWrap: "wrap",
          gap: { row: 5, column: 5 },
        },
        asset: {
          minWidth: 200,
          maxWidth: 240,
          flex: 1,
          height: "auto",
          border: {
            color: "#000",
            thickness: 1,
          },
          alignItems: "center",
          padding: { vertical: 5 },
          gap: { row: 5 },
        },
        symbol: {
          font: { name: "roboto-regular", size: 24 },
        },
      }),
    []
  );

  return (
    <XFrames.Node style={styleSheet.mainWrapperNode}>
      {cryptoAssets.map((asset) => {
        return (
          <XFrames.Node key={asset.id} style={styleSheet.asset}>
            <XFrames.UnformattedText
              style={styleSheet.symbol}
              text={asset.symbol}
            />
            {/* <XFrames.ItemTooltip>
                            <XFrames.UnformattedText text={asset.name} />
                        </XFrames.ItemTooltip> */}

            {/** Fix tooltip! */}
            {/* <HelpMarker text={asset.name} /> */}

            <CryptoQuotePrice symbol={asset.symbol} />
          </XFrames.Node>
        );
      })}
    </XFrames.Node>
  );
};
