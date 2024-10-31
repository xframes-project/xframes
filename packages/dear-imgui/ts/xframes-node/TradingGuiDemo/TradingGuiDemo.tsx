import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { subDays, subHours, subMinutes } from "date-fns";
import { Subscribe } from "@react-rxjs/core";
import {
    GetCryptoBarsParams,
    GetQuotesParams,
} from "@alpacahq/alpaca-trade-api/dist/resources/datav2/rest_v2";
// import { HelpMarker } from "./HelpMarker/HelpMarker";
import { CryptoLinePlots } from "./CryptoPlots/CryptoLinePlots";
import { useStore } from "./store";
import { CryptoAssetsList } from "./CryptoAssetsList/CryptoAssetsList";
import { DataService } from "./dataService";
import { DataServiceContext } from "./dataServiceContext";
import { cryptoSymbols } from "./cryptoSymbols";
import { CryptoAssetPanels } from "./CryptoAssetPanels/CryptoAssetPanels";
import { CryptoCandlestickPlots } from "./CryptoPlots/CryptoCandlestickPlots";
import { Sidebar } from "./Sidebar/Sidebar";
import { CryptoSymbolPair } from "./CryptoSymbolPairs/CryptoSymbolPair";
import { CryptoSymbolBlock } from "./CryptoSymbolBlock/CryptoSymbolBlock";
import { Tabs } from "./Tabs/Tabs";
import { XFrames } from "../XFrames";
import { useWidgetRegistrationService } from "../../src/lib/hooks/useWidgetRegistrationService";
import RWStyleSheet from "../../src/lib/stylesheet/stylesheet";
import { ImGuiCol, ImGuiStyleVar } from "../../src/lib/wasm/wasm-app-types";
import { theme2Colors } from "../../src/lib/stylesheet/themes";
import faIconMap from "../../src/lib/fa-icons";

const componentMap = {
    cryptoAssetPanels: CryptoAssetPanels,
    cryptoAssetList: CryptoAssetsList,
    cryptoLinePlots: CryptoLinePlots,
    cryptoCandlestickPlots: CryptoCandlestickPlots,
};

const dataService = new DataService();

type ComponentKeys = keyof typeof componentMap;

export const TradingGuiDemo = () => {
    const symbols = useStore((state) => state.symbols);
    const selectedTabIndex = useStore((state) => state.selectedTabIndex);
    const setSelectedTabIndex = useStore((state) => state.setSelectedTabIndex);
    const setCryptoAssets = useStore((state) => state.setCryptoAssets);

    // const socketRef = useRef<WebSocket>();
    const widgetRegistratonService = useWidgetRegistrationService();

    const [selectedItemIds, setSelectedItemIds] = useState<ComponentKeys[]>(["cryptoAssetPanels"]);

    const styleSheet = useMemo(
        () =>
            RWStyleSheet.create({
                rootNode: {
                    height: "100%",
                },
                mainLayoutNode: {
                    flex: 1,
                    flexDirection: "row",
                },
                title: {
                    colors: { [ImGuiCol.Text]: theme2Colors.green },
                    font: { name: "roboto-regular", size: 24 },
                },
                debugButton: {
                    positionType: "absolute",
                    position: { right: 15, bottom: 15 },
                    flexDirection: "row",
                    gap: { column: 10 },
                },
                button: {
                    colors: {
                        [ImGuiCol.Text]: theme2Colors.green,
                        [ImGuiCol.Border]: theme2Colors.green,
                    },
                    padding: { all: 8 },
                    vars: {
                        [ImGuiStyleVar.FrameBorderSize]: 1,
                        [ImGuiStyleVar.FrameRounding]: 5,
                    },
                },
                mainNode: {
                    flex: 1,
                },
                mainTitleNode: {
                    flexDirection: "row",
                    borderBottom: {
                        color: "#1B1D20", // todo: remove double border
                        thickness: 1,
                    },
                    borderRight: {
                        color: "#1B1D20", // todo: remove double border
                        thickness: 1,
                    },
                    padding: { all: 20 },
                },
                mainTitle: {
                    font: { name: "roboto-bold", size: 32 },
                },
                contentNode: {
                    padding: {
                        left: 8,
                    },
                },
                tabContent: {
                    height: "100%",
                    width: 350,
                    gap: { row: 10 },
                    padding: {
                        horizontal: 8,
                        vertical: 16,
                    },
                    borderLeft: {
                        color: "#1C1E22",
                        thickness: 1,
                    },
                },
                marketSearchInput: {
                    vars: {
                        [ImGuiStyleVar.FrameBorderSize]: 1,
                        [ImGuiStyleVar.FramePadding]: [10, 10],
                    },
                    colors: {
                        [ImGuiCol.Border]: "#1C1E22",
                    },
                },
                marketSearchInputHover: {
                    vars: {
                        [ImGuiStyleVar.FrameBorderSize]: 1,
                        [ImGuiStyleVar.FramePadding]: [10, 10],
                    },
                    colors: {
                        [ImGuiCol.Border]: "#fff",
                        [ImGuiCol.TextDisabled]: "#fff",
                    },
                },
                marketSearchFilterWrapper: {
                    flexDirection: "row",
                    gap: { all: 8 },
                    flexWrap: "wrap",
                },
                marketSearchFilter: {
                    width: 60,
                    colors: {
                        [ImGuiCol.Button]: "#1E2025",
                        [ImGuiCol.ButtonHovered]: "#1E2025",
                        [ImGuiCol.ButtonActive]: "#1E2025",
                        [ImGuiCol.Text]: "#777B84",
                    },
                    vars: {
                        [ImGuiStyleVar.FrameRounding]: 5,
                    },
                },
                marketSearchFilterActive: {
                    width: 60,
                    colors: {
                        [ImGuiCol.Button]: "#fff",
                        [ImGuiCol.ButtonHovered]: "#fff",
                        [ImGuiCol.ButtonActive]: "#fff",
                        [ImGuiCol.Text]: "#292A2C",
                    },
                    vars: {
                        [ImGuiStyleVar.FrameRounding]: 5,
                    },
                },
            }),
        [],
    );

    // todo: move to dataService.ts ?
    // const connect = useCallback(() => {
    //     socketRef.current = new WebSocket("ws://localhost:4000");

    //     socketRef.current.addEventListener("error", () => {
    //         console.log("Connected to server");
    //     });

    //     socketRef.current.addEventListener("open", () => {
    //         console.log("Connected to server");
    //     });

    //     socketRef.current.addEventListener("close", (ev) => {
    //         try {
    //             const reason = JSON.parse(ev.reason);

    //             console.log(`Disconnected from server: ${reason.message}`);
    //         } catch (error) {
    //             console.log("Disconnected from server");
    //         }
    //     });

    //     socketRef.current.addEventListener("message", (event) => {
    //         const data = JSON.parse(event.data);

    //         if (data.cryptoAssets) {
    //             // todo: remove filtering
    //             setCryptoAssets(
    //                 data.cryptoAssets.filter((asset: any) => cryptoSymbols.includes(asset.symbol)),
    //             );
    //         } else if (data.cryptoQuote) {
    //             dataService.addCryptoQuote(data.cryptoQuote);
    //         } else if (data.latestCryptoQuotes) {
    //             // console.log(data.latestCryptoQuotes);

    //             Object.entries(data.latestCryptoQuotes).forEach(([symbol, cryptoQuote]) =>
    //                 dataService.addCryptoQuote({ ...(cryptoQuote as any), S: symbol }),
    //             );
    //         } else if (data.cryptoSnapshots) {
    //             dataService.addCryptoSnapshots(data.cryptoSnapshots);
    //             // console.log(data.cryptoSnapshots);
    //         } else if (data.latestCryptoBars) {
    //             console.log(data.latestCryptoBars);
    //         } else if (data.cryptoBars) {
    //             dataService.addCryptoBars(data.cryptoBars);
    //             // console.log(data.cryptoBars);
    //         }
    //     });
    // }, [setCryptoAssets]);

    // const sendMessage = useCallback((message: any) => {
    //     if (socketRef.current) {
    //         socketRef.current.send(JSON.stringify(message));
    //     }
    // }, []);

    // const subscribeToLiveData = useCallback(
    //     () =>
    //         sendMessage({
    //             passkey: "",
    //             action: "subscribeForCryptoQuotes",
    //             symbols,
    //         }),
    //     [sendMessage, symbols],
    // );

    // const getLatestQuotes = useCallback(() => {
    //     const currentDate = new Date();
    //     const end = currentDate.toISOString();
    //     const start = subMinutes(currentDate, 1);

    //     sendMessage({
    //         action: "getQuotes",
    //         symbols,
    //         options: { start, end },
    //     });
    // }, [symbols]);

    // const getCryptoQuotes = useCallback(() => {
    //     const currentDate = new Date();
    //     const end = currentDate.toISOString();
    //     const start = subMinutes(currentDate, 1).toISOString();
    //     const options: GetQuotesParams = {
    //         start,
    //         end,
    //     };

    //     sendMessage({
    //         action: "getCryptoQuotes",
    //         symbols,
    //         options,
    //     });
    // }, [symbols]);

    // const getCryptoBars = useCallback(() => {
    //     const currentDate = new Date();
    //     const end = currentDate.toISOString();
    //     const start = subDays(currentDate, 30).toISOString();
    //     const options: GetCryptoBarsParams = { start, end, timeframe: "1D" };

    //     sendMessage({
    //         action: "getCryptoBars",
    //         symbols,
    //         options,
    //     });
    // }, [symbols]);

    // const getCryptoSnapshots = useCallback(
    //     () =>
    //         sendMessage({
    //             action: "getCryptoSnapshots",
    //             symbols,
    //         }),
    //     [symbols],
    // );

    // const getLatestCryptoQuotes = useCallback(
    //     () =>
    //         sendMessage({
    //             action: "getLatestCryptoQuotes",
    //             symbols,
    //         }),
    //     [symbols],
    // );

    // const getLatestCryptoBars = useCallback(
    //     () =>
    //         sendMessage({
    //             action: "getLatestCryptoBars",
    //             symbols,
    //         }),
    //     [symbols],
    // );

    // const getCryptoAssets = useCallback(
    //     () =>
    //         sendMessage({
    //             action: "getCryptoAssets",
    //             symbols,
    //         }),
    //     [symbols],
    // );

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

    // useEffect(() => {
    // connect();
    // setTimeout(() => {
    //     getCryptoAssets();
    // }, 1000);
    // }, [connect, getCryptoAssets]);

    const tabs = useMemo(() => ["Markets", "History"], []);

    const Component = componentMap[selectedItemIds[0]];

    return (
        <XFrames.Node root style={styleSheet.rootNode}>
            {/* <XFrames.UnformattedText text="Trading GUI demo" style={styleSheet.title} /> */}

            {/* <XFrames.Node
                style={{
                    width: "100%",
                    flexDirection: "row",
                    alignItems: "center",
                    gap: { column: 15 },
                }}
            >
                <XFrames.Button onClick={connect} label="Connect" style={styleSheet.button} />
                <XFrames.Button
                    onClick={subscribeToLiveData}
                    label="Subscribe to live data"
                    style={styleSheet.button}
                />
                <XFrames.Button
                    onClick={getCryptoAssets}
                    label="Get Crypto Assets"
                    style={styleSheet.button}
                />
                <XFrames.Button
                    onClick={getCryptoQuotes}
                    label="Get Crypto Quotes"
                    style={styleSheet.button}
                />
                <XFrames.Button
                    onClick={getCryptoSnapshots}
                    label="Get Crypto Snapshots"
                    style={styleSheet.button}
                />
                <XFrames.Button
                    onClick={getCryptoBars}
                    label="Get Crypto Bars"
                    style={styleSheet.button}
                />
                <XFrames.Button
                    onClick={getLatestCryptoQuotes}
                    label="Get Latest Crypto Quotes"
                    style={styleSheet.button}
                />
                <XFrames.Button
                    onClick={getLatestCryptoBars}
                    label="Get Latest Crypto Bars"
                    style={styleSheet.button}
                />
            </XFrames.Node> */}

            <XFrames.Node style={styleSheet.mainLayoutNode}>
                <Sidebar />
                <XFrames.Node style={styleSheet.mainNode}>
                    <XFrames.Node style={styleSheet.mainTitleNode}>
                        <XFrames.UnformattedText text="Trade" style={styleSheet.mainTitle} />
                    </XFrames.Node>

                    <XFrames.Node style={styleSheet.contentNode}>
                        <CryptoSymbolBlock symbol="BTC/USD" />

                        <Tabs
                            tabs={tabs}
                            selectedTabIndex={selectedTabIndex}
                            onSelectedTabChange={setSelectedTabIndex}
                        />

                        <XFrames.Node style={styleSheet.tabContent}>
                            <XFrames.InputText
                                hint={`${faIconMap["magnifying-glass"]} SEARCH MARKETS`}
                                style={styleSheet.marketSearchInput}
                                hoverStyle={styleSheet.marketSearchInputHover}
                            />
                            <XFrames.Node style={styleSheet.marketSearchFilterWrapper}>
                                <XFrames.Button
                                    label={faIconMap["star"]}
                                    style={styleSheet.marketSearchFilter}
                                    hoverStyle={styleSheet.marketSearchFilterActive}
                                />
                                <XFrames.Button
                                    label="ALL"
                                    style={styleSheet.marketSearchFilterActive}
                                    hoverStyle={styleSheet.marketSearchFilterActive}
                                />
                                <XFrames.Button
                                    label="TOP"
                                    style={styleSheet.marketSearchFilter}
                                    hoverStyle={styleSheet.marketSearchFilterActive}
                                />
                                <XFrames.Button
                                    label="BTC"
                                    style={styleSheet.marketSearchFilter}
                                    hoverStyle={styleSheet.marketSearchFilterActive}
                                />
                                <XFrames.Button
                                    label="ETH"
                                    style={styleSheet.marketSearchFilter}
                                    hoverStyle={styleSheet.marketSearchFilterActive}
                                />
                                <XFrames.Button
                                    label="USD"
                                    style={styleSheet.marketSearchFilter}
                                    hoverStyle={styleSheet.marketSearchFilterActive}
                                />
                                <XFrames.Button
                                    label="USDC"
                                    style={styleSheet.marketSearchFilter}
                                    hoverStyle={styleSheet.marketSearchFilterActive}
                                />
                                <XFrames.Button
                                    label="DAI"
                                    style={styleSheet.marketSearchFilter}
                                    hoverStyle={styleSheet.marketSearchFilterActive}
                                />
                                <XFrames.Button
                                    label="MKR"
                                    style={styleSheet.marketSearchFilter}
                                    hoverStyle={styleSheet.marketSearchFilterActive}
                                />
                            </XFrames.Node>
                        </XFrames.Node>
                    </XFrames.Node>
                </XFrames.Node>

                {/* <XFrames.Node style={styleSheet.contentNode} cull={false}>
                    {Component && (
                        <DataServiceContext.Provider value={dataService}>
                            <Component />
                        </DataServiceContext.Provider>
                    )}
                </XFrames.Node> */}
            </XFrames.Node>
            <XFrames.Node style={styleSheet.sidebarNode}></XFrames.Node>

            <XFrames.Node style={styleSheet.debugButton}>
                <XFrames.Button label={faIconMap.bug} onClick={debugModeBtnClicked} />
            </XFrames.Node>
        </XFrames.Node>
    );
};
