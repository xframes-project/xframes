import React, { useEffect, useMemo, useRef } from "react";
import { useDataService } from "../dataServiceContext";
import { TableImperativeHandle } from "../../../src/lib/components/ReactImgui/Table";
import { ReactImgui } from "../../ReactImgui";

type Props = {};

export const CryptoAssetsList = ({}: Props) => {
    const dataService = useDataService();
    const tableRef = useRef<TableImperativeHandle>(null);

    const tableColumns = useMemo(
        () => [
            {
                heading: "Symbol",
                fieldId: "symbol",
            },
            {
                heading: "ASK",
                fieldId: "askPrice",
            },
            {
                heading: "Change",
                fieldId: "askChange",
            },
            {
                heading: "BID",
                fieldId: "bidPrice",
            },
            {
                heading: "Change",
                fieldId: "bidChange",
            },
        ],
        [],
    );

    useEffect(() => {
        const subscription = dataService.getCryptoSnapshots().subscribe((snapshots) => {
            const data = Object.entries(snapshots).map(([symbol, snapshot]) => ({
                symbol,
                askPrice: snapshot.LatestQuote.AskPrice.toFixed(6),
                bidPrice: snapshot.LatestQuote.BidPrice.toFixed(6),
                // todo: totally unsure here
                askChange: (snapshot.LatestQuote.AskPrice - snapshot.DailyBar.Open).toFixed(6),
                bidChange: (snapshot.LatestQuote.BidPrice - snapshot.DailyBar.Open).toFixed(6),
            }));
            data.sort((a, b) => {
                if (a.symbol < b.symbol) {
                    return -1;
                } else if (a.symbol > b.symbol) {
                    return 1;
                }

                return 0;
            });

            tableRef.current?.setTableData(data);
        });

        return () => {
            subscription.unsubscribe();
        };
    }, [dataService]);

    return <ReactImgui.Table ref={tableRef} columns={tableColumns} />;
};
