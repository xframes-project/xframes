import React, { createContext, useCallback, useEffect, useMemo, useRef, useState } from "react";
import { subMinutes } from "date-fns";
import { Subscribe } from "@react-rxjs/core";
import { ReactImgui } from "src/lib/components/ReactImgui/components";
import { useWidgetRegistrationService } from "src/lib/hooks";
import { ImGuiCol, ImGuiStyleVar } from "src/lib/wasm/wasm-app-types";
// import { HelpMarker } from "./HelpMarker/HelpMarker";
import faIconMap from "../../fa-icons";
import RWStyleSheet from "../../stylesheet/stylesheet";
import { Plots } from "./Plots/Plots";
import { TreeViewItem } from "../ReactImgui/TreeView";
import { useStore } from "./store";
import { CryptoAssetsList } from "./CryptoAssetsList/CryptoAssetsList";
import { DataService } from "./dataService";
import { DataServiceContext } from "./dataServiceContext";
import { cryptoSymbols } from "./cryptoSymbols";

const componentMap = {
    cryptoAssetsList: CryptoAssetsList,
    // plots: Plots,
};

const dataService = new DataService();

type ComponentKeys = keyof typeof componentMap;

export const TradingGuiDemo = () => {
    const symbols = useStore((state) => state.symbols);
    const setCryptoAssets = useStore((state) => state.setCryptoAssets);

    const socketRef = useRef<WebSocket>();
    const widgetRegistratonService = useWidgetRegistrationService();

    const [selectedItemIds, setSelectedItemIds] = useState<ComponentKeys[]>(["cryptoAssetsList"]);

    const treeViewItems: TreeViewItem[] = useMemo(() => {
        return [
            {
                itemId: "cryptoAssetsList",
                label: "Crypto Assets List",
            },
            {
                itemId: "plots",
                label: "Plots",
            },
        ];
    }, []);

    const styleSheet = useMemo(
        () =>
            RWStyleSheet.create({
                rootNode: {
                    height: "100%",
                    padding: {
                        all: 10,
                    },
                    gap: { row: 12 },
                },
                mainLayoutNode: {
                    flex: 1,
                    flexDirection: "row",
                    gap: { column: 12 },
                },
                sidebarNode: {
                    flexBasis: 200,
                    height: "100%",
                    borderColor: "#000",
                    borderThickness: 1,
                },
                contentNode: {
                    flex: 1,
                    height: "100%",
                    borderColor: "#000",
                    borderThickness: 1,
                    padding: { all: 5 },
                },
                title: {
                    colors: { [ImGuiCol.Text]: "#ff6e59" },
                    font: { name: "roboto-regular", size: 24 },
                },
                debugButton: {
                    positionType: "absolute",
                    position: { right: 15, bottom: 15 },
                    flexDirection: "row",
                    gap: { column: 10 },
                },
            }),
        [],
    );

    const connect = useCallback(() => {
        socketRef.current = new WebSocket("ws://localhost:4000");

        socketRef.current.addEventListener("error", () => {
            console.log("Connected to server");
        });

        socketRef.current.addEventListener("open", () => {
            console.log("Connected to server");
        });

        socketRef.current.addEventListener("close", (ev) => {
            try {
                const reason = JSON.parse(ev.reason);

                console.log(`Disconnected from server: ${reason.message}`);
            } catch (error) {
                console.log("Disconnected from server");
            }
        });

        socketRef.current.addEventListener("message", (event) => {
            const data = JSON.parse(event.data);

            if (data.cryptoAssets) {
                // todo: remove filtering
                setCryptoAssets(
                    data.cryptoAssets.filter((asset: any) => cryptoSymbols.includes(asset.symbol)),
                );
            } else if (data.cryptoQuote) {
                dataService.addCryptoQuote(data.cryptoQuote);
            } else if (data.latestCryptoQuotes) {
                // console.log(data.latestCryptoQuotes);

                Object.entries(data.latestCryptoQuotes).forEach(([symbol, cryptoQuote]) =>
                    dataService.addCryptoQuote({ ...(cryptoQuote as any), S: symbol }),
                );
            }
        });
    }, [setCryptoAssets]);

    const subscribeToLiveData = useCallback(() => {
        // BTC/USD,ETH/USD
        if (socketRef.current) {
            const message = JSON.stringify({
                passkey: "",
                action: "subscribeForCryptoQuotes",
                symbols,
            });

            // console.log(message);

            socketRef.current.send(message);
        }
    }, [symbols]);

    const getLatestQuotes = useCallback(() => {
        // BTC/USD,ETH/USD
        if (socketRef.current) {
            const currentDate = new Date();
            const end = currentDate.toISOString();
            const start = subMinutes(currentDate, 1);

            const message = JSON.stringify({
                passkey: "",
                action: "getQuotes",
                symbols,
                options: { start, end },
            });

            // console.log(message);

            socketRef.current.send(message);
        }
    }, [symbols]);

    const getCryptoQuotes = useCallback(() => {
        // BTC/USD,ETH/USD
        if (socketRef.current) {
            const currentDate = new Date();
            const end = currentDate.toISOString();
            const start = subMinutes(currentDate, 1);

            const message = JSON.stringify({
                passkey: "",
                action: "getCryptoQuotes",
                symbols,
                options: { start, end },
            });

            // console.log(message);

            socketRef.current.send(message);
        }
    }, [symbols]);

    const getLatestCryptoQuotes = useCallback(() => {
        // BTC/USD,ETH/USD
        if (socketRef.current) {
            // const currentDate = new Date();
            // const end = currentDate.toISOString();
            // const start = subMinutes(currentDate, 1);

            const message = JSON.stringify({
                passkey: "",
                action: "getLatestCryptoQuotes",
                symbols,
                // options: { start, end },
            });

            // console.log(message);

            socketRef.current.send(message);
        }
    }, [symbols]);

    const getCryptoAssets = useCallback(
        (symbols?: string[]) => {
            if (socketRef.current) {
                const message = JSON.stringify({
                    passkey: "",
                    action: "getCryptoAssets",
                    symbols,
                });

                socketRef.current.send(message);
            }
        },
        [symbols],
    );

    const debugModeBtnClicked = useCallback(() => {
        widgetRegistratonService.setDebug(true);
    }, []);

    const onToggleItemSelection = useCallback((itemId: string, selected: boolean) => {
        setSelectedItemIds((selection) => {
            if (selected) {
                return [itemId as ComponentKeys];
            } else {
                return selection.filter((item) => item !== itemId);
            }
        });
    }, []);

    useEffect(() => {
        connect();

        setTimeout(() => {
            getCryptoAssets();
        }, 1000);
    }, [connect, getCryptoAssets]);

    const Component = componentMap[selectedItemIds[0]];

    return (
        <ReactImgui.Node root style={styleSheet.rootNode}>
            <ReactImgui.UnformattedText text="Trading GUI demo" style={styleSheet.title} />

            <ReactImgui.Node
                style={{
                    width: "100%",
                    flexDirection: "row",
                    alignItems: "center",
                    gap: { column: 15 },
                }}
            >
                <ReactImgui.Button onClick={connect} label="Connect" />
                <ReactImgui.Button onClick={subscribeToLiveData} label="Subscribe to live data" />
                <ReactImgui.Button onClick={getCryptoAssets} label="Get Crypto Assets" />
                <ReactImgui.Button onClick={getLatestQuotes} label="Get Latest Quotes" />
                <ReactImgui.Button onClick={getCryptoQuotes} label="Get Crypto Quotes" />
                <ReactImgui.Button
                    onClick={getLatestCryptoQuotes}
                    label="Get Latest Crypto Quotes"
                />
            </ReactImgui.Node>

            <ReactImgui.Node style={styleSheet.mainLayoutNode}>
                <ReactImgui.Node style={styleSheet.sidebarNode}>
                    <ReactImgui.TreeView
                        items={treeViewItems}
                        selectedItemIds={selectedItemIds}
                        onToggleItemSelection={onToggleItemSelection}
                    />
                </ReactImgui.Node>
                <ReactImgui.Node style={styleSheet.contentNode} cull={false}>
                    {Component && (
                        <DataServiceContext.Provider value={dataService}>
                            <Component />
                        </DataServiceContext.Provider>
                    )}
                </ReactImgui.Node>
            </ReactImgui.Node>

            <ReactImgui.Node style={styleSheet.debugButton}>
                <ReactImgui.Button label={faIconMap.bug} onClick={debugModeBtnClicked} />
            </ReactImgui.Node>
        </ReactImgui.Node>
    );
};