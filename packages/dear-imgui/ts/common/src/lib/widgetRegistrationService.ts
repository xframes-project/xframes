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
