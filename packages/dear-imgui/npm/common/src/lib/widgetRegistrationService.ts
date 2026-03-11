import { v4 as uuidv4 } from "uuid";
// import { MainModule } from "./wasm/wasm-app-types";
import { XFramesStyle } from "./stylesheet/xframes-style";
import { PlotCandlestickDataItem } from "./components/XFrames/types";

export class WidgetRegistrationService {
    private wasmModule: any;
    private tables: Set<string>;
    private maps: Set<string>;
    private fabricWidgetsMapping: Map<string, number>;
    private fonts: string[];

    constructor(wasmModule: any) {
        this.wasmModule = wasmModule;
        this.tables = new Set();
        this.maps = new Set();
        this.fabricWidgetsMapping = new Map();
        this.fonts = [];
    }

    setFonts(fonts: string[]) {
        this.fonts = fonts;
    }

    getFonts() {
        return this.fonts;
    }

    getStyle(): XFramesStyle {
        return JSON.parse(this.wasmModule.getStyle());
    }

    generateId() {
        return uuidv4();
    }

    linkWidgetIds(id: string, fabricId: number) {
        this.fabricWidgetsMapping.set(id, fabricId);
    }

    unlinkWidgetIds(id: string) {
        this.fabricWidgetsMapping.delete(id);
    }

    setDebug(debug: boolean) {
        this.wasmModule.setDebug(debug);
    }

    showDebugWindow() {
        this.wasmModule.showDebugWindow();
    }

    registerTable(id: string) {
        this.tables.add(id);
    }

    unregisterTable(id: string) {
        this.tables.delete(id);
    }

    registerMap(id: string) {
        this.maps.add(id);
    }

    unregisterMap(id: string) {
        this.maps.delete(id);
    }

    appendDataToTable(id: string, data: any[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "appendData", data }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    setTableData(id: string, data: any[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setData", data }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    appendDataToPlotLine(id: string, x: number, y: number) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "appendData", x, y }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    appendSeriesDataToPlotLine(id: string, seriesIndex: number, x: number, y: number) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "appendSeriesData", seriesIndex, x, y }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    setPlotLineData(id: string, series: { data: { x: number; y: number }[] }[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setData", series }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    setPlotLineAxesDecimalDigits(id: string, x: number, y: number) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setAxesDecimalDigits", x, y }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    setPlotLineAutoAxisFitEnabled(id: string, enabled: boolean) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setAxesAutoFit", enabled }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    setPlotBarData(id: string, data: { x: number; y: number }[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setData", data }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    setPlotHeatmapData(id: string, rows: number, cols: number, values: number[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setData", rows, cols, values }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    setPlotPieChartData(id: string, data: { label: string; value: number }[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setData", data }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    setPlotHistogramData(id: string, values: number[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setData", data: values }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    appendDataToPlotHistogram(id: string, value: number) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "appendData", value }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    setPlotScatterData(id: string, data: { x: number; y: number }[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setData", data }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    setPlotCandlestickData(id: string, data: PlotCandlestickDataItem[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setData", data }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    // todo: 'merge'?
    setPlotCandlestickAutoAxisFitEnabled(id: string, enabled: boolean) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setAxesAutoFit", enabled }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    resetPlotData(id: string) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "resetData" }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    reloadImage(id: string) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "reloadImage" }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    renderMap(id: string, centerX: number, centerY: number, zoom: number) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "render", centerX, centerY, zoom }),
                );
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    prefetchMapTiles(id: string, minLon: number, minLat: number, maxLon: number, maxLat: number, minZoom: number, maxZoom: number) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "prefetch", minLon, minLat, maxLon, maxLat, minZoom, maxZoom }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    setMapMarkers(id: string, markers: { lat: number; lon: number; color?: string; label?: string; radius?: number }[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setMarkers", markers }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    clearMapMarkers(id: string) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "clearMarkers" }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    setMapPolylines(id: string, polylines: { points: { lat: number; lon: number }[]; color?: string; thickness?: number }[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setPolylines", polylines }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    clearMapPolylines(id: string) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "clearPolylines" }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    setMapOverlays(id: string, overlays: { lat: number; lon: number; radiusMeters: number; radiusMinorMeters?: number; rotation?: number; fillColor?: string; strokeColor?: string; strokeThickness?: number }[]) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setOverlays", overlays }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    clearMapOverlays(id: string) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "clearOverlays" }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    appendMapPolylinePoint(id: string, polylineIndex: number, lat: number, lon: number) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "appendPolylinePoint", polylineIndex, lat, lon }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    appendTextToClippedMultiLineTextRenderer(id: string, text: string) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.appendTextToClippedMultiLineTextRenderer(fabricWidgetId, text);
            } catch (error) {
                // todo: propagate this?
                console.error(error);
            }
        }
    }

    setInputTextValue(id: string, value: string) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setValue", value }),
            );
        }
    }

    setColumnFilter(id: string, columnIndex: number, filterText: string) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "setColumnFilter", columnIndex, filterText }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    clearTableFilters(id: string) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            try {
                this.wasmModule.elementInternalOp(
                    fabricWidgetId,
                    JSON.stringify({ op: "clearFilters" }),
                );
            } catch (error) {
                console.error(error);
            }
        }
    }

    setSliderValue(id: string, value: number) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setValue", value }),
            );
        }
    }

    setComboSelectedIndex(id: string, index: number) {
        const fabricWidgetId = this.fabricWidgetsMapping.get(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setSelectedIndex", index }),
            );
        }
    }
}
