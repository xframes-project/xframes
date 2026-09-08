import { v4 as uuidv4 } from "uuid";
// import { MainModule } from "./wasm/wasm-app-types";
import { XFramesStyle } from "./stylesheet/xframes-style";
import { PlotCandlestickDataItem } from "./components/XFrames/types";

export type WidgetTarget = { readonly nativeId: number; alive: boolean; publicId?: string };
type WidgetId = string | WidgetTarget | undefined;
export type RegistrationKind = "table" | "map" | "widget";

export class WidgetRegistrationService {
    private wasmModule: any;
    private tables: Set<string>;
    private maps: Set<string>;
    private fabricWidgetsMapping: Map<string, number>;
    private fonts: string[];
    private nativeWidgets = new Map<number, WidgetTarget>();
    private registrations = new Map<WidgetTarget, Map<RegistrationKind, Set<object>>>();
    private droppedOperations = 0;
    private disposed = false;

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

    getDiagnostics() {
        return {
            mappings: [...this.fabricWidgetsMapping.entries()].map(([publicId, nativeId]) => ({
                publicId,
                nativeId,
            })),
            mappingCount: this.fabricWidgetsMapping.size,
            tableCount: this.tables.size,
            mapCount: this.maps.size,
            reverseMappingCount: [...this.nativeWidgets.values()].filter(target => target.publicId !== undefined).length,
            nativeCount: this.nativeWidgets.size,
            registrationCount: this.registrations.size,
            droppedOperations: this.droppedOperations,
            disposed: this.disposed,
        };
    }

    getStyle(): XFramesStyle {
        return JSON.parse(this.wasmModule.getStyle());
    }

    generateId() {
        return uuidv4();
    }

    createNativeTarget(nativeId: number) {
        if (this.disposed) return;
        if (!this.nativeWidgets.has(nativeId)) this.nativeWidgets.set(nativeId, { nativeId, alive: true });
    }

    linkWidgetIds(id: string, fabricId: number) {
        // Direct binding users may link an already-created native widget without
        // going through Fabric. Native liveness is the authority for that case.
        if (!this.disposed && !this.nativeWidgets.has(fabricId) && this.wasmModule?.isElementAlive?.(fabricId)) {
            this.createNativeTarget(fabricId);
        }
        const target = this.nativeWidgets.get(fabricId);
        if (!target?.alive) return;
        this.unlinkNativePublicId(target);
        const previous = this.captureWidget(id);
        if (previous) this.unlinkNativePublicId(previous);
        target.publicId = id;
        this.fabricWidgetsMapping.set(id, fabricId);
        this.refreshRegistrationSets(target);
    }

    private unlinkNativePublicId(target: WidgetTarget) {
        const id = target.publicId;
        if (id !== undefined && this.fabricWidgetsMapping.get(id) === target.nativeId) {
            this.fabricWidgetsMapping.delete(id);
            this.tables.delete(id);
            this.maps.delete(id);
        }
        target.publicId = undefined;
    }

    unlinkWidgetIds(id: string, owner = this.captureWidget(id)) {
        if (owner && owner.publicId === id) this.unlinkNativePublicId(owner);
    }

    setPublicId(nativeId: number, id: unknown) {
        const target = this.nativeWidgets.get(nativeId);
        if (!target) return;
        if (typeof id === "string") this.linkWidgetIds(id, nativeId);
        else this.unlinkNativePublicId(target);
    }

    releaseNativeTarget(nativeId: number) {
        const target = this.nativeWidgets.get(nativeId);
        if (!target) return;
        target.alive = false;
        this.unlinkNativePublicId(target);
        this.registrations.delete(target);
        this.nativeWidgets.delete(nativeId);
    }

    captureWidget(id: string): WidgetTarget | undefined {
        const nativeId = this.fabricWidgetsMapping.get(id);
        return nativeId === undefined ? undefined : this.nativeWidgets.get(nativeId);
    }

    private resolveTarget(id: WidgetId) {
        const target = typeof id === "string" ? this.captureWidget(id) : id;
        if (target?.alive && this.nativeWidgets.get(target.nativeId) === target && !this.disposed) {
            if (!this.wasmModule.isElementAlive || this.wasmModule.isElementAlive(target.nativeId)) return target;
            this.releaseNativeTarget(target.nativeId);
        }
        return undefined;
    }

    private getNativeId(id: WidgetId) {
        const target = this.resolveTarget(id);
        if (!target) this.droppedOperations = Math.min(Number.MAX_SAFE_INTEGER, this.droppedOperations + 1);
        return target?.nativeId;
    }

    private refreshRegistrationSets(target: WidgetTarget) {
        const id = target.publicId;
        if (id === undefined || this.fabricWidgetsMapping.get(id) !== target.nativeId) return;
        const kinds = this.registrations.get(target);
        for (const [kind, set] of [["table", this.tables], ["map", this.maps]] as const) {
            if (kinds?.get(kind)?.size) set.add(id);
            else set.delete(id);
        }
    }

    // A lease owns only this setup. Strict Mode and delayed cleanup can release an
    // earlier lease without changing a later setup or a rebound public ID.
    registerWidget(id: WidgetId, kind: RegistrationKind = "widget"): () => void {
        const target = this.resolveTarget(id);
        if (!target) return () => {};
        const kinds = this.registrations.get(target) ?? new Map<RegistrationKind, Set<object>>();
        const owners = kinds.get(kind) ?? new Set<object>();
        const owner = {};
        owners.add(owner);
        kinds.set(kind, owners);
        this.registrations.set(target, kinds);
        this.refreshRegistrationSets(target);
        return () => {
            owners.delete(owner);
            if (!owners.size && kinds.get(kind) === owners) kinds.delete(kind);
            if (!kinds.size && this.registrations.get(target) === kinds) this.registrations.delete(target);
            this.refreshRegistrationSets(target);
        };
    }

    destroy() {
        this.disposed = true;
        for (const nativeId of this.nativeWidgets.keys()) this.releaseNativeTarget(nativeId);
        this.wasmModule = undefined;
        this.fonts = [];
    }

    setDebug(debug: boolean) {
        this.wasmModule.setDebug(debug);
    }

    showDebugWindow() {
        this.wasmModule.showDebugWindow();
    }

    registerTable(id: WidgetId) {
        return this.registerWidget(id, "table");
    }

    unregisterTable(id: WidgetId) {
        const target = this.resolveTarget(id);
        if (target) {
            this.registrations.get(target)?.delete("table");
            if (!this.registrations.get(target)?.size) this.registrations.delete(target);
            this.refreshRegistrationSets(target);
        }
    }

    registerMap(id: WidgetId) {
        return this.registerWidget(id, "map");
    }

    unregisterMap(id: WidgetId) {
        const target = this.resolveTarget(id);
        if (target) {
            this.registrations.get(target)?.delete("map");
            if (!this.registrations.get(target)?.size) this.registrations.delete(target);
            this.refreshRegistrationSets(target);
        }
    }

    appendDataToTable(id: WidgetId, data: any[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "appendData", data }),
            );
        }
    }

    setTableData(id: WidgetId, data: any[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setData", data }),
            );
        }
    }

    appendDataToPlotLine(id: WidgetId, x: number, y: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "appendData", x, y }),
            );
        }
    }

    appendSeriesDataToPlotLine(id: WidgetId, seriesIndex: number, x: number, y: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "appendSeriesData", seriesIndex, x, y }),
            );
        }
    }

    setPlotLineData(id: WidgetId, series: { data: { x: number; y: number }[] }[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setData", series }),
            );
        }
    }

    setPlotLineAxesDecimalDigits(id: WidgetId, x: number, y: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setAxesDecimalDigits", x, y }),
            );
        }
    }

    setPlotLineAutoAxisFitEnabled(id: WidgetId, enabled: boolean) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setAxesAutoFit", enabled }),
            );
        }
    }

    setPlotBarData(id: WidgetId, data: { x: number; y: number }[], tickLabels?: string[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            const op: Record<string, unknown> = { op: "setData", data };
            if (tickLabels) op.tickLabels = tickLabels;
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify(op),
            );
        }
    }

    setPlotBarSeriesData(id: WidgetId, seriesData: { data: { x: number; y: number }[]; tickLabels?: string[] }[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setSeriesData", series: seriesData }),
            );
        }
    }

    appendPlotBarSeriesData(id: WidgetId, seriesIndex: number, x: number, y: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "appendSeriesData", seriesIndex, x, y }),
            );
        }
    }

    setPlotHeatmapData(id: WidgetId, rows: number, cols: number, values: number[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setData", rows, cols, values }),
            );
        }
    }

    setPlotPieChartData(id: WidgetId, data: { label: string; value: number }[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setData", data }),
            );
        }
    }

    setPlotHistogramData(id: WidgetId, values: number[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setData", data: values }),
            );
        }
    }

    appendDataToPlotHistogram(id: WidgetId, value: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "appendData", value }),
            );
        }
    }

    setPlotScatterData(id: WidgetId, data: { x: number; y: number }[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setData", data }),
            );
        }
    }

    setPlotCandlestickData(id: WidgetId, data: PlotCandlestickDataItem[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setData", data }),
            );
        }
    }

    // todo: 'merge'?
    setPlotCandlestickAutoAxisFitEnabled(id: WidgetId, enabled: boolean) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setAxesAutoFit", enabled }),
            );
        }
    }

    resetPlotData(id: WidgetId) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "resetData" }),
            );
        }
    }

    reloadImage(id: WidgetId) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "reloadImage" }),
            );
        }
    }

    renderMap(id: WidgetId, centerX: number, centerY: number, zoom: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "render", centerX, centerY, zoom }),
            );
        }
    }

    prefetchMapTiles(id: WidgetId, minLon: number, minLat: number, maxLon: number, maxLat: number, minZoom: number, maxZoom: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "prefetch", minLon, minLat, maxLon, maxLat, minZoom, maxZoom }),
            );
        }
    }

    setMapMarkers(id: WidgetId, markers: { lat: number; lon: number; color?: string; label?: string; radius?: number }[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setMarkers", markers }),
            );
        }
    }

    clearMapMarkers(id: WidgetId) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "clearMarkers" }),
            );
        }
    }

    setMapPolylines(id: WidgetId, polylines: { points: { lat: number; lon: number }[]; color?: string; thickness?: number }[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setPolylines", polylines }),
            );
        }
    }

    clearMapPolylines(id: WidgetId) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "clearPolylines" }),
            );
        }
    }

    setMapOverlays(id: WidgetId, overlays: { lat: number; lon: number; radiusMeters: number; radiusMinorMeters?: number; rotation?: number; fillColor?: string; strokeColor?: string; strokeThickness?: number }[]) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setOverlays", overlays }),
            );
        }
    }

    clearMapOverlays(id: WidgetId) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "clearOverlays" }),
            );
        }
    }

    appendMapPolylinePoint(id: WidgetId, polylineIndex: number, lat: number, lon: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "appendPolylinePoint", polylineIndex, lat, lon }),
            );
        }
    }

    appendTextToClippedMultiLineTextRenderer(id: WidgetId, text: string) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.appendTextToClippedMultiLineTextRenderer(fabricWidgetId, text);
        }
    }

    setInputTextValue(id: WidgetId, value: string) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setValue", value }),
            );
        }
    }

    setColumnFilter(id: WidgetId, columnIndex: number, filterText: string) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setColumnFilter", columnIndex, filterText }),
            );
        }
    }

    clearTableFilters(id: WidgetId) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "clearFilters" }),
            );
        }
    }

    setSliderValue(id: WidgetId, value: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setValue", value }),
            );
        }
    }

    setCanvasContinuous(id: WidgetId, continuous: boolean) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(fabricWidgetId, JSON.stringify({ op: "setContinuous", continuous }));
        }
    }

    redrawCanvas(id: WidgetId) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(fabricWidgetId, JSON.stringify({ op: "redraw" }));
        }
    }

    setCanvasScript(id: WidgetId, script: string) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setScript", script }),
            );
        }
    }

    setCanvasScriptFile(id: WidgetId, path: string) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setScriptFile", path }),
            );
        }
    }

    setCanvasData(id: WidgetId, data: any) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setData", data }),
            );
        }
    }

    clearCanvas(id: WidgetId) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "clear" }),
            );
        }
    }

    loadCanvasTexture(id: WidgetId, textureId: string, source: string) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "loadTexture", textureId, source }),
            );
        }
    }

    unloadCanvasTexture(id: WidgetId, textureId: string) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "unloadTexture", textureId }),
            );
        }
    }

    reloadCanvasTexture(id: WidgetId, textureId: string, source: string) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "reloadTexture", textureId, source }),
            );
        }
    }

    setComboSelectedIndex(id: WidgetId, index: number) {
        const fabricWidgetId = this.getNativeId(id);
        if (fabricWidgetId !== undefined) {
            this.wasmModule.elementInternalOp(
                fabricWidgetId,
                JSON.stringify({ op: "setSelectedIndex", index }),
            );
        }
    }
}
