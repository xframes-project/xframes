import React, { RefObject, useCallback, useEffect, useRef, useState } from "react";
import { PlotLineImperativeHandle } from "../../XFrames/PlotLine";
import { ImPlotScale } from "src/lib/wasm/wasm-app-types";
import { useDataService } from "../dataServiceContext";
import { filter } from "rxjs";
import { XFrames } from "../../XFrames";

type Props = { symbol: string };

export const CryptoLinePlot = ({ symbol }: Props) => {
    const dataService = useDataService();

    const plotRef = useRef<PlotLineImperativeHandle>(null);

    // const [axisAutoFit, setAxisAutoFit] = useState(true);

    useEffect(() => {
        const subscription = dataService
            .getCryptoQuotes()
            .pipe(filter((cryptoQuote) => cryptoQuote.S === symbol))
            .subscribe((quote) => {
                if (plotRef.current) {
                    const date = new Date(quote.Timestamp);

                    plotRef.current.appendData(Number(date) / 1000, quote.BidPrice);
                }
            });

        return () => {
            subscription.unsubscribe();
        };
    }, [dataService]);

    // const toggleAxisAutoFit = useCallback(() => {
    //     setAxisAutoFit((val) => !val);
    // }, []);

    return (
        <XFrames.PlotLine
            xAxisScale={ImPlotScale.Time}
            ref={plotRef}
            style={{ width: "100%", height: 400 }}
            // axisAutoFit={axisAutoFit}
        />
    );
};
