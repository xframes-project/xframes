import { createRequire } from "node:module";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { mkdirSync, writeFileSync } from "node:fs";
import { EventEmitter } from "node:events";
import { cpus, platform, release } from "node:os";
import { check, waitFor } from "./assertions";
import { observeNativeFrame, readDiagnostics, waitForNativeIdle, counterDelta } from "./frames";

/** Run against an isolated ubx-monitor checkout installed with packed current
 * XFrames and React 19. No serial device or original app configuration is opened. */
async function main() {
    check(process.env.XFRAMES_UBX_APP_DIR, "Set XFRAMES_UBX_APP_DIR to the isolated validation checkout");
    const app = resolve(process.env.XFRAMES_UBX_APP_DIR);
    const output = resolve(process.env.XFRAMES_DIAGNOSTICS_DIR ?? "build/diagnostics/ubx-telemetry");
    mkdirSync(output, { recursive: true });
    process.chdir(app); // App config.ts writes only this isolated config.json.
    const requireApp = createRequire(resolve(app, "package.json"));
    const React = requireApp("react");
    const common = requireApp("@xframes/common");
    check(React.version === "19.2.3", "External application must use the same React version as XFrames");
    const native = requireApp("@xframes/node/dist/xframes.node");
    const { UbxParser } = requireApp("ubx-parser");
    const { SignalStrengthPanel } = await import(pathToFileURL(resolve(app, "src/panels/SignalStrengthPanel.tsx")).href);
    const { serialManager } = await import(pathToFileURL(resolve(app, "src/connection/SerialManager.ts")).href);
    let plotUpdates = 0;
    const observed = Object.create(native, { elementInternalOp: { value: (id: number, wire: string) => {
        if (JSON.parse(wire).op === "setSeriesData") plotUpdates++;
        return native.elementInternalOp(id, wire);
    } } });
    const host = common.createReactNativeHost();
    const service = new common.WidgetRegistrationService(observed);
    host.nativeFabricUIManager.init(observed, service);
    const renderer = common.ReactFabricInitialiser(host);
    const render = (element: unknown) => new Promise<void>((done, reject) => renderer.render(element == null ? null
        : React.createElement(common.WidgetRegistrationServiceContext.Provider, { value: service }, element), 0,
        () => { try { host.nativeFabricUIManager.assertPublicationHealthy(); done(); } catch (error) { reject(error); } }, 1,
        { onUncaughtError: reject }));
    await new Promise<void>(done => native.init({ assetsBasePath: resolve(app, "assets"), theme: "{}",
        fontDefs: JSON.stringify({ defs: [{ name: "roboto-regular", size: 16 }, { name: "roboto-mono", size: 14 }] }), onInit: done,
        ...Object.fromEntries(["onTextChange", "onComboChange", "onNumericValueChange", "onBooleanValueChange", "onMultiValueChange",
            "onClick", "onTableSort", "onTableFilter", "onTableRowClick", "onTableItemAction", "onPrefetchProgress", "onScriptError", "onBeforeExit"].map(name => [name, () => {}])) }));
    native.setDiagnosticsEnabled(true);
    const initial = await waitForNativeIdle(native, "external application initial inactivity");
    const listeners = serialManager.listenerCount("NAV-SAT");
    await render(React.createElement("node", { root: true, style: { width: 900, height: 700 } }, React.createElement(SignalStrengthPanel)));
    await waitFor(() => serialManager.listenerCount("NAV-SAT"), count => count === listeners + 1, "actual useNavSat subscription");
    // Substitute the physical port only. Exercise the application's own
    // setupPortListeners -> UbxParser.feed -> NAV-SAT -> hook -> PlotBar path.
    const port = new EventEmitter();
    serialManager.port = port;
    serialManager.parser = new UbxParser();
    serialManager.setupPortListeners();
    let messages = 0;
    const receipt = (message: any) => { check(message.list.length === 4, "Parser lost synthetic satellites"); messages++; };
    serialManager.on("NAV-SAT", receipt);
    const packet = (sequence: number) => {
        const data = Buffer.alloc(8 + 8 + 4 * 12);
        data.set([0xb5, 0x62, 1, 0x35]); data.writeUInt16LE(data.length - 8, 4);
        data.writeUInt32LE(sequence * 50, 6); data[10] = 1; data[11] = 4;
        for (let index = 0; index < 4; ++index) {
            const offset = 14 + index * 12;
            data[offset] = [0, 2, 3, 6][index]; data[offset + 1] = index + 1;
            data[offset + 2] = 15 + index * 10 + sequence % 4; data[offset + 3] = 30;
            data.writeInt16LE(index * 60, offset + 4); data.writeUInt32LE(8, offset + 8);
        }
        let a = 0, b = 0;
        for (const byte of data.subarray(2, -2)) { a = (a + byte) & 255; b = (b + a) & 255; }
        data[data.length - 2] = a; data[data.length - 1] = b;
        return data;
    };
    const latencies: number[] = [];
    const feed = async (sequence: number) => {
        const start = performance.now();
        const data = packet(sequence);
        // Deliberately split a UBX packet across two actual port data callbacks.
        port.emit("data", data.subarray(0, 9)); port.emit("data", data.subarray(9));
        await observeNativeFrame(native, frame => frame.elements.some(node => node.type === "plot-bar"
            && node.state.series.length === 4 && node.state.series.every((series: any, index: number) =>
                series.count === 1 && series.lastX === index && series.lastY === 15 + index * 10 + sequence % 4)),
            `external CNO sample ${sequence} reaches all four quality bands`);
        latencies.push(performance.now() - start);
    };
    await feed(0);
    const idle = await waitForNativeIdle(native, "external application idle before sustained telemetry");
    const streamStart = performance.now();
    for (let sequence = 1; sequence <= 200; ++sequence) {
        await feed(sequence);
        await new Promise(resolveDelay => setTimeout(resolveDelay, Math.max(0, streamStart + sequence * 50 - performance.now())));
    }
    const streamElapsedMs = performance.now() - streamStart;
    const quiet = await waitForNativeIdle(native, "external telemetry settles");
    await new Promise(resolveDelay => setTimeout(resolveDelay, 1000));
    const afterQuiet = readDiagnostics(native);
    check(afterQuiet.scheduler.constructed === quiet.scheduler.constructed && afterQuiet.scheduler.submitted === quiet.scheduler.submitted,
        "External application kept rendering during idle");
    await feed(201);
    check(messages === 202 && plotUpdates === 202, "External parser/PlotBar update accounting differs");
    await new Promise<void>((done, reject) => native.captureScreenshot(resolve(output, "telemetry.png"), (error: string) => error ? reject(new Error(error)) : done()));
    await render(null);
    await observeNativeFrame(native, frame => frame.elementCount === 0 && frame.internalSubjectCount === 0, "external application cleanup");
    serialManager.off("NAV-SAT", receipt);
    await waitFor(() => serialManager.listenerCount("NAV-SAT"), count => count === listeners,
        "useNavSat passive-effect cleanup releases its application listener");
    const beforeLate = readDiagnostics(native).scheduler.generation;
    port.emit("data", packet(202));
    check(readDiagnostics(native).scheduler.generation === beforeLate, "Unmounted telemetry revived native state");
    serialManager.disconnect(); port.removeAllListeners();
    renderer.stopSurface(0); host.nativeFabricUIManager.destroy();
    const final = await waitForNativeIdle(native, "external cleanup inactivity");
    check(final.scheduler.ownerCount === initial.scheduler.ownerCount, "External application retained scheduler owners");
    const sorted = [...latencies].sort((a, b) => a - b);
    const percentile = (fraction: number) => sorted[Math.ceil(sorted.length * fraction) - 1];
    writeFileSync(resolve(output, "result.json"), JSON.stringify({ status: "passed", source: app, react: React.version,
        metadata: { node: process.version, os: `${platform()} ${release()}`, cpu: cpus()[0]?.model,
            timingContext: process.env.XFRAMES_UBX_TIMING_CONTEXT ?? "See accompanying build/run evidence" },
        input: "Synthetic UBX NAV-SAT packets through the actual SerialManager port-data and parser path; no physical serial device",
        messages, plotUpdates, latencyMs: latencies, sustainedSamples: 200, requestedHz: 20,
        observedLatencyMs: { samples: latencies.length, p50: percentile(.5), p95: percentile(.95), p99: percentile(.99), maximum: percentile(1) },
        streamElapsedMs, frames: counterDelta(final.scheduler.submitted, idle.scheduler.submitted),
        initial, final, listenersAfterCleanup: serialManager.listenerCount("NAV-SAT") }, null, 2));
    console.log("External ubx-monitor telemetry: passed");
}
const watchdog = setTimeout(() => { console.error("External telemetry watchdog expired"); process.exit(1); }, 90_000);
main().then(() => { clearTimeout(watchdog); process.exit(0); }).catch(error => { console.error(error); clearTimeout(watchdog); process.exit(1); });
