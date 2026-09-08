import wasmData from "../wasm/src/lib/xframes.data";
import { defaults, runRuntime, type RunOptions } from "./runtime";
import { waitFor } from "./assertions";
import { verifyWasmWrapper } from "./wasm-wrapper";
import type { FixtureInput, FixtureEvent } from "./input";
import { verifyShutdown } from "./shutdown";
import { check } from "./assertions";

declare const XFRAMES_DIAGNOSTICS_OPTIONS: Partial<RunOptions>;
declare const XFRAMES_SOURCE_REVISION: string;
declare const XFRAMES_ADAPTER: string;
declare const XFRAMES_NATIVE_BUILD: string;
declare const XFRAMES_HOST_INFO: object;
const state = { status: "initializing", captureRequested: false, captureDone: false, report: null as any, error: null as string | null,
    inputRequest: null as (FixtureInput & { id: number }) | null, inputDone: 0 };
(globalThis as any).__xframesDiagnostics = state;
const start = performance.now();

async function main() {
    const { default: getWasmModule } = await import("../wasm/src/lib/xframes.mjs");
    document.body.replaceChildren();
    document.body.style.margin = "0";
    const canvas = document.createElement("canvas");
    canvas.id = "xframes-diagnostics";
    canvas.width = 900;
    canvas.height = 700;
    document.body.append(canvas);
    let markReady: () => void;
    const ready = new Promise<void>(resolve => { markReady = resolve; });
    const handlers = Object.fromEntries(["onTextChange", "onComboChange", "onNumericValueChange", "onBooleanValueChange", "onMultiValueChange",
        "onClick", "onTableSort", "onTableFilter", "onTableRowClick", "onTableItemAction", "onPrefetchProgress"].map(name => [name, () => {}]));
    const prefetchEvents: { id: number; completed: number; total: number }[] = [];
    const inputEvents: FixtureEvent[] = [];
    let exited = false;
    const recordInput = (event: FixtureEvent) => {
        if (inputEvents.length === 256) throw new Error("Input fixture event capacity exceeded");
        inputEvents.push(event);
    };
    const listenerAudit = (globalThis as any).__xframesListenerAudit;
    check(listenerAudit, "Browser smoke must install the DOM listener audit");
    listenerAudit.phase("native");
    const native: any = await getWasmModule({ canvas,
        arguments: ["#xframes-diagnostics", JSON.stringify({ defs: [{ name: "roboto-regular", size: 16 }] }), "{}"],
        locateFile: () => wasmData,
        eventHandlers: { ...handlers, onInit: () => markReady(), onScriptError: (...args: unknown[]) => { throw new Error(JSON.stringify(args)); },
            onPrefetchProgress: (id: number, completed: number, total: number) => {
                if (prefetchEvents.length === 256) throw new Error("Prefetch fixture event capacity exceeded");
                prefetchEvents.push({ id, completed, total });
            }, onTextChange: (id: number, value: string) => recordInput({ kind: "text", id, value }),
            onNumericValueChange: (id: number, value: number) => recordInput({ kind: "number", id, value }) },
        onAbort: (message: unknown) => { state.status = "failed"; state.error = `Wasm abort: ${message}`; },
        onExit: (code: number) => { check(code === 0, `Unexpected native exit ${code}`); exited = true; },
    });
    await ready;
    listenerAudit.phase("page");
    const nativeListeners = listenerAudit.snapshot();
    check(nativeListeners.native > 0, "Native initialization registered no audited DOM listeners");
    console.log("ready");
    state.status = "running";
    const readyMs = performance.now() - start;
    const gpuAdapter = await (navigator as any).gpu?.requestAdapter();
    const report = await runRuntime(native, { ...defaults, ...XFRAMES_DIAGNOSTICS_OPTIONS }, {
        capture: async () => {
            state.captureRequested = true;
            await waitFor(() => state.captureDone, Boolean, "CDP populated fixture capture", 20_000);
        },
        report: report => { state.report = report; },
        prefetchEvents: () => prefetchEvents,
        input: async request => {
            const id = state.inputDone + 1;
            state.inputRequest = { ...request, id };
            await waitFor(() => state.inputDone, done => done === id, `CDP ${request.action} input`, 20_000);
        }, events: () => inputEvents,
        resources: () => ({ sampledAtMs: performance.now(), rssBytes: null, cpuMicroseconds: null,
            availability: "Total browser/native resident memory and CPU are unavailable in the page; the native module does not export heap capacity",
            wasmHeapCapacityBytes: null }),
        metadata: { runtime: "Wasm/WebGPU", sourceRevision: XFRAMES_SOURCE_REVISION, host: XFRAMES_HOST_INFO, renderer: process.env.NODE_ENV,
            nativeBuild: XFRAMES_NATIVE_BUILD, toolchain: "Docker/Emscripten 5.0.2", userAgent: navigator.userAgent,
            adapterSelection: XFRAMES_ADAPTER, adapterInfo: gpuAdapter?.info ? { vendor: gpuAdapter.info.vendor, architecture: gpuAdapter.info.architecture,
                device: gpuAdapter.info.device, description: gpuAdapter.info.description } : null,
            readyMs, assets: "repository roboto-regular.ttf, size 16", display: "900x700 canvas", devicePixelRatio },
    });
    report.wrapperLifecycle = await verifyWasmWrapper(native);
    check(listenerAudit.snapshot().native === nativeListeners.native, "Wrapper reuse accumulated native DOM listeners");
    report.shutdown = await verifyShutdown(native, { ...defaults, ...XFRAMES_DIAGNOSTICS_OPTIONS }.resourceFixture!, async () => {
        try { native.exit(); }
        catch (error: any) { check(error?.name === "ExitStatus" && error.status === 0, `Unexpected Wasm exit error: ${error}`); }
        await waitFor(() => exited, Boolean, "Wasm native exit callback");
    }, () => native.shutdownDiagnostics);
    report.browserListeners = { initial: nativeListeners, final: listenerAudit.snapshot() };
    check(report.browserListeners.final.native === 0, `Native teardown retained DOM listeners: ${JSON.stringify(report.browserListeners.final)}`);
    state.report = report;
    state.status = "complete";
    console.log(`Wasm diagnostics: ${report.status}`);
}

main().catch(error => { state.error = String(error); state.status = "failed"; console.error(error); });
