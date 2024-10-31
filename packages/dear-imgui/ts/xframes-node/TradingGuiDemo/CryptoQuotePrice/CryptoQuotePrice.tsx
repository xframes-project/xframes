import React, { useEffect, useMemo, useState } from "react";
import { useDataService } from "../dataServiceContext";
import { filter } from "rxjs";
import { interval, intervalToDuration, formatDuration, differenceInMilliseconds } from "date-fns";
import { CryptoQuoteWithSymbol } from "../dataService";
import { ReactImgui } from "../../ReactImgui";
import RWStyleSheet from "../../../src/lib/stylesheet/stylesheet";
import { theme2Colors } from "../../../src/lib/stylesheet/themes";
import { ImGuiCol } from "../../../src/lib/wasm/wasm-app-types";

type Props = { symbol: string };

type LocalData = {
    quote?: CryptoQuoteWithSymbol;
    lastUpdated: string;
    bidPriceDiretion?: "up" | "down";
    askPriceDiretion?: "up" | "down";
};

export const CryptoQuotePrice = ({ symbol }: Props) => {
    const [data, setData] = useState<LocalData>({
        lastUpdated: "n/a",
    });
    // const [lastUpdated, setLastUpdated] = useState<string>("n/a");

    const dataService = useDataService();

    useEffect(() => {
        const subscription = dataService
            .getCryptoQuotes()
            .pipe(filter((cryptoQuote) => cryptoQuote.S === symbol))
            .subscribe((quote) => {
                const newData: LocalData = {
                    quote,
                    lastUpdated: "n/a",
                };

                setData((oldData) => {
                    // if (oldData.quote) {
                    //     const quoteDate = new Date(quote.Timestamp);
                    //     const previousDate = new Date(oldData.quote.Timestamp);
                    //     const msDiff = differenceInMilliseconds(quoteDate, previousDate);

                    //     if (msDiff >= 1000) {
                    //         newData.lastUpdated = formatDuration(
                    //             intervalToDuration(interval(quoteDate, previousDate)),
                    //         );
                    //     } else {
                    //         newData.lastUpdated = `${msDiff}ms ago`;
                    //     }
                    // }

                    return newData;
                });
            });

        return () => {
            subscription.unsubscribe();
        };
    }, [dataService]);

    const styleSheet = useMemo(
        () =>
            RWStyleSheet.create({
                pricesWrapper: {
                    alignSelf: "center",
                    flexDirection: "row",
                    gap: { column: 5 },
                },
                price: {
                    width: 100,
                    height: "auto",
                    // borderColor: "#000",
                    // borderThickness: 1,
                    alignItems: "center",
                },
                ask: {
                    colors: {
                        [ImGuiCol.Text]: theme2Colors.red,
                    },
                },
                bid: {
                    colors: {
                        [ImGuiCol.Text]: theme2Colors.green,
                    },
                },
            }),
        [],
    );

    return (
        <>
            <ReactImgui.Node style={styleSheet.pricesWrapper}>
                <ReactImgui.Node style={styleSheet.price}>
                    <ReactImgui.UnformattedText text="ASK" style={styleSheet.ask} />
                    <ReactImgui.UnformattedText
                        text={
                            typeof data?.quote?.AskPrice === "number"
                                ? data?.quote?.AskPrice.toFixed(6)
                                : "n/a"
                        }
                        style={styleSheet.ask}
                    />
                </ReactImgui.Node>
                <ReactImgui.Node style={styleSheet.price}>
                    <ReactImgui.UnformattedText text="BID" style={styleSheet.bid} />
                    <ReactImgui.UnformattedText
                        text={
                            typeof data?.quote?.BidPrice === "number"
                                ? data?.quote?.BidPrice.toFixed(6)
                                : "n/a"
                        }
                        style={styleSheet.bid}
                    />
                </ReactImgui.Node>
            </ReactImgui.Node>
            {/* <ReactImgui.UnformattedText text={data.lastUpdated} /> */}
        </>
    );
};
