import { createRequire } from "node:module";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { mkdirSync, writeFileSync } from "node:fs";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { EventEmitter } from "node:events";
import { cpus, platform, release } from "node:os";
import { check, waitFor } from "./assertions";
import { observeNativeFrame, readDiagnostics, counterDelta, waitForNativeIdle } from "./frames";
import { navSatPacket, navPvtPacket } from "./ubx-bytes";

const execute = promisify(execFile);
const here = __dirname;
const output = resolve(process.env.XFRAMES_DIAGNOSTICS_DIR ?? "build/diagnostics/ubx-application/probe");
mkdirSync(output, { recursive: true });
const save = (name: string, data: unknown) => writeFileSync(resolve(output, `${name}.json`), JSON.stringify(data, null, 2));
const delay = (ms: number) => new Promise<void>(done => setTimeout(done, ms));
let native: any;
let finalReport: any;

async function main() {
    check(process.env.XFRAMES_UBX_APP_DIR, "Set XFRAMES_UBX_APP_DIR to the isolated application");
    const app = resolve(process.env.XFRAMES_UBX_APP_DIR);
    const requireApp = createRequire(resolve(app, "package.json"));
    const { startResourceServer } = await import("./resource-server.mjs");
    const resources = await startResourceServer(resolve(here, ".."), output);
    process.env.UBX_MONITOR_TILE_URL = `${resources.baseUrl}/asset?group=app&hold=1&z={z}&x={x}&y={y}`;
    process.env.UBX_MONITOR_TILE_CACHE = resolve(output, "tile-cache");
    await fetch(`${resources.controlUrl}/release?group=app`);
    process.chdir(app);
    // This is an isolated checkout: each run starts with deterministic UI choices
    // and never imports the original receiver's machine-local configuration.
    writeFileSync(resolve(app, "config.json"), "{}\n");
    native = requireApp("@xframes/node/dist/xframes.node");
    const common = requireApp("@xframes/common");
    const manager = common.ReactNativePrivateInterface.nativeFabricUIManager;
    let baseline: any;
    const init = native.init;
    native.init = (options: any) => init({ ...options, onInit: (...args: any[]) => {
        baseline = readDiagnostics(native);
        options.onInit(...args);
    }, onBeforeExit: () => {
        if (finalReport) {
            try {
                const terminal = readDiagnostics(native);
                check(terminal.scheduler.status === "disposed" && terminal.scheduler.ownerCount === 0
                    && terminal.scheduler.activeOwners === 0 && terminal.scheduler.deadlines === 0
                    && !terminal.scheduler.wakeAttached && !terminal.scheduler.notificationPending,
                    "Native shutdown retained scheduler ownership");
                check(Object.values(terminal.platform).every(value => value === 0), "Native shutdown retained platform callbacks");
                check(terminal.resourceState.textures.liveTextures === 0 && terminal.resourceState.textures.retiredTextures === 0,
                    "Native shutdown retained textures");
                save("result", { ...finalReport, status: "passed", terminal });
                console.log("Ordinary ubx-monitor application: passed");
            } catch (error) { save("failure", { error: String(error), native: readDiagnostics(native) }); process.exit(1); }
        }
        options.onBeforeExit();
    } });
    const nodes = new Map<number, any>();
    const operations: Record<string, number> = {};
    const latest = new Map<number, any>();
    const highWater: Record<string, number> = {};
    const inputRetries: string[] = [];
    const apply = native.applyCommit;
    native.applyCommit = (wire: string) => {
        const result = apply(wire);
        const ack = JSON.parse(result);
        check(ack.status === "applied", `Application publication failed: ${result}`);
        for (const op of JSON.parse(wire).operations) {
            if (op.op === "create") nodes.set(op.id, { type: op.elementType, ...op.props });
            if (op.op === "patch") Object.assign(nodes.get(op.id), op.props);
        }
        for (const id of ack.destroyedIds) { nodes.delete(id); latest.delete(id); }
        return result;
    };
    const internal = native.elementInternalOp;
    native.elementInternalOp = (id: number, wire: string) => {
        const result = internal(id, wire);
        const op = JSON.parse(wire);
        const key = `${nodes.get(id)?.type}:${op.op}`;
        operations[key] = (operations[key] ?? 0) + 1;
        latest.set(id, { ...latest.get(id), [op.op]: op });
        return result;
    };
    const { serialManager } = await import(pathToFileURL(resolve(app, "src/connection/SerialManager.ts")).href);
    const appIntervals = new Set<any>();
    const originalSetInterval = globalThis.setInterval, originalClearInterval = globalThis.clearInterval;
    globalThis.setInterval = ((...args: any[]) => {
        const timer = (originalSetInterval as any)(...args);
        if (new Error().stack?.replaceAll("\\", "/").includes(app.replaceAll("\\", "/") + "/src/")) appIntervals.add(timer);
        return timer;
    }) as typeof setInterval;
    globalThis.clearInterval = ((timer: any) => { appIntervals.delete(timer); originalClearInterval(timer); }) as typeof clearInterval;
    class Port extends EventEmitter {
        isOpen = true;
        writes: Buffer[] = [];
        close(callback?: (error: Error | null) => void) {
            this.isOpen = false;
            queueMicrotask(() => { this.emit("close"); callback?.(null); });
        }
        write(data: Buffer) { this.writes.push(Buffer.from(data)); return true; }
    }
    const ports: Port[] = [];
    serialManager.setTransport({ list: async () => [{ path: "SYNTHETIC", manufacturer: "Deterministic UBX fixture" }],
        open: (_options: unknown, callback: (error: Error | null) => void) => {
            const port = new Port(); ports.push(port); queueMicrotask(() => callback(null)); return port;
        } });
    const input = async (action: string, x = 50, y = 24, value?: string) => {
        check(process.platform === "win32", "This bounded application fixture currently supports Windows input");
        if (action === "click") { await input("move", x, y); await delay(200); }
        await execute("powershell.exe", ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", resolve(here, "native-window.ps1"),
            "-ProcessId", String(process.pid), "-Action", action, "-X", String(x), "-Y", String(y),
            ...(value === undefined ? [] : ["-Value", value])], { windowsHide: true, timeout: 10_000 });
    };
    const entry = await import(pathToFileURL(resolve(app, "src/index.tsx")).href);
    await waitFor(() => manager.getDiagnostics(), (d: any) => d.appliedPublications > 0, "ordinary App publication");
    native.setDiagnosticsEnabled(true);
    native.resizeWindow(1280, 900);
    const initial = await observeNativeFrame(native, f => f.elementCount > 30, "complete App startup");
    const service = manager.widgetRegistrationService;
    const semantic = () => ({ nodes: [...nodes].map(([id, props]) => ({ id, ...props })), latest: [...latest], operations,
        serial: serialManager.eventNames().map((event: string) => [event, serialManager.listenerCount(event)]),
        bridge: manager.getDiagnostics(), registrations: service.getDiagnostics() });
    const capture = async (name: string) => {
        await new Promise<void>((done, reject) => native.captureScreenshot(resolve(output, `${name}.png`),
            (error: string) => error ? reject(new Error(error)) : done()));
        save(name, { native: readDiagnostics(native), semantic: semantic() });
    };
    const visit = async (label: string, x: number) => {
        const id = [...nodes].find(([, props]) => props.type === "tab-item" && props.label === label)?.[0];
        check(id !== undefined, `Missing application tab ${label}`);
        for (let attempt = 1; attempt <= 3; ++attempt) {
            await input("click", x, 43);
            try {
                await waitFor(() => readDiagnostics(native), f => (f.elements?.find(n => n.id === id)?.bounds[2] ?? 0) > 0,
                    `${label} visible content`, 2000);
                return;
            } catch (error) {
                inputRetries.push(`${label}:${attempt}`);
                console.log(`Input retry ${attempt}: ${label}`);
                if (attempt === 3) throw error;
            }
        }
    };
    const clickUntil = async (label: string, x: number, y: number, predicate: () => boolean) => {
        for (let attempt = 1; attempt <= 3; ++attempt) {
            if (predicate()) return;
            await input("click", x, y);
            try { await waitFor(predicate, Boolean, label, 2000); return; }
            catch (error) { inputRetries.push(`${label}:${attempt}`); if (attempt === 3) throw error; }
        }
    };
    const snapshot = () => readDiagnostics(native);
    const element = (type: string) => snapshot().elements?.find(node => node.type === type);
    const hasText = (test: RegExp) => [...nodes.values()].some(props => props.type === "unformatted-text" && test.test(props.text));
    if (process.argv.includes("--scenario")) {
        check(process.env.NODE_ENV === "production", "Acceptance scenario requires production React");
        console.log("Desktop interaction scenario: mouse and focus will be used; missed targets are retried.");
        const receipts = { sat: new Map<number, number>(), pvt: new Map<number, number>() };
        const observations = { sat: new Map<number, any>(), pvt: new Map<number, any>() };
        let messageCount = 0, rawCallbacks = 0, rawBytes = 0, splitPairs = 0, coalescedPairs = 0;
        const decoded = (kind: "sat" | "pvt") => (msg: any) => {
            const sequence = msg.iTOW / 50;
            check(Number.isInteger(sequence) && !receipts[kind].has(sequence), `Duplicate/invalid ${kind} sequence ${sequence}`);
            receipts[kind].set(sequence, performance.now());
        };
        const onSat = decoded("sat"), onPvt = decoded("pvt");
        const onMessage = () => ++messageCount;
        const onRaw = (chunk: Buffer) => { rawCallbacks++; rawBytes += chunk.length; };
        serialManager.on("NAV-SAT", onSat); serialManager.on("NAV-PVT", onPvt);
        serialManager.on("message", onMessage); serialManager.on("rawdata", onRaw);
        const listenerCounts = () => Object.fromEntries(serialManager.eventNames().map((event: string) => [event, serialManager.listenerCount(event)]));
        const subscriptions = listenerCounts();
        const parsers: any[] = [];
        const connect = async () => {
            await visit("Connection", 46);
            await clickUntil("Connect callback", 44, 140, () => serialManager.getStatus() === "connected");
            parsers.push(serialManager.parser);
            check(JSON.stringify(listenerCounts()) === JSON.stringify(subscriptions), "Connection use accumulated application subscriptions");
        };
        const send = (sequence: number) => {
            const port = ports.at(-1)!;
            check(port.isOpen, "Telemetry source lost its connection");
            const sat = navSatPacket(sequence), pvt = navPvtPacket(sequence);
            if (sequence % 2) {
                splitPairs++;
                port.emit("data", sat.subarray(0, 9));
                port.emit("data", Buffer.concat([sat.subarray(9), pvt]));
            } else { coalescedPairs++; port.emit("data", Buffer.concat([sat, pvt])); }
        };
        let observationError: unknown;
        const inspect = () => {
            try {
                const frame = snapshot();
                for (const node of frame.elements ?? []) {
                    const state = node.state;
                    if (!state) continue;
                    if (node.type === "di-table") { check(state.rowCount <= 500, "Message history exceeded 500 rows"); highWater.messages = Math.max(highWater.messages ?? 0, state.rowCount); }
                    if (["plot-line", "plot-scatter"].includes(node.type)) {
                        check(state.pointCount <= state.pointsLimit * (state.seriesCount ?? 1), "Position history exceeded its declared limit");
                        highWater[node.type] = Math.max(highWater[node.type] ?? 0, state.pointCount);
                    }
                    if (node.type === "clipped-multi-line-text-renderer") {
                        check(state.byteCount <= 3 * 65536, "Console history exceeded its UTF-8 bound");
                        highWater.consoleBytes = Math.max(highWater.consoleBytes ?? 0, state.byteCount);
                    }
                    if (node.type === "map-view") {
                        check(state.polylinePoints <= 1000, "Map trail exceeded 1000 points");
                        highWater.mapTrail = Math.max(highWater.mapTrail ?? 0, state.polylinePoints);
                    }
                }
                const series = frame.elements?.find(n => n.type === "plot-bar")?.state?.series;
                const marker = frame.elements?.find(n => n.type === "map-view")?.state?.lastMarker;
                const values: ["sat" | "pvt", number | undefined][] = [
                    ["sat", series?.length === 4 && series.every((s: any) => s.count === 1)
                        ? series.reduce((sum: number, s: any, i: number) => sum + (s.lastY - 10 * (i + 1)) * 10 ** i, 0) : undefined],
                    ["pvt", marker ? Math.round((marker.lon + .12) * 1e6) : undefined],
                ];
                for (const [kind, sequence] of values) {
                    if (sequence === undefined || !receipts[kind].has(sequence) || observations[kind].has(sequence)) continue;
                    observations[kind].set(sequence, { sequence, latencyMs: performance.now() - receipts[kind].get(sequence)!, frameId: frame.frameId });
                }
                highWater.parserBytes = Math.max(highWater.parserBytes ?? 0, serialManager.parser?._buffer.length ?? 0);
            } catch (error) { observationError = error; }
        };
        const poll = setInterval(inspect, 10);
        await connect();
        send(0);
        const streamBefore = snapshot();
        const cpuBefore = process.cpuUsage(), memoryBefore = process.memoryUsage().rss;
        const start = performance.now();
        const delivery: number[] = [];
        // This source timer never waits for a frame, screenshot, or interaction.
        const stream = new Promise<void>((done, reject) => {
            const tick = (sequence: number) => setTimeout(() => {
                try {
                    send(sequence); delivery.push(performance.now() - start);
                    if (sequence === 1200) done(); else tick(sequence + 1);
                } catch (error) { reject(error); }
            }, Math.max(0, start + sequence * 50 - performance.now()));
            tick(1);
        });
        await visit("Signals", 505);
        await observeNativeFrame(native, f => f.elements.some(n => n.type === "plot-bar" && n.state?.series.every((s: any) => s.count === 1)), "four CNO bands");
        await capture("signals");
        await clickUntil("CNO sort callback", 282, 84, () => {
            const series = element("plot-bar")?.state?.series;
            return series?.length === 4 && series.every((s: any, i: number) => s.lastX === 3 - i);
        });
        await capture("signals-sorted");
        await visit("Messages", 188);
        await observeNativeFrame(native, f => f.elements.some(n => n.type === "di-table" && n.state.rowCount > 0), "populated message table");
        await capture("messages");
        await visit("Map", 330);
        await observeNativeFrame(native, f => f.elements.some(n => n.type === "map-view" && (n.resources?.loadedTextures ?? 0) > 0
            && n.state.markerCount === 1 && n.state.overlayCount === 1 && n.state.polylinePoints > 0), "populated local Map with overlays");
        await capture("map");
        const zoomBefore = element("map-view")?.state.zoom;
        for (let attempt = 1; attempt <= 3; ++attempt) {
            await input("move", 640, 450); await delay(200); await input("wheel", 640, 450);
            try { await waitFor(() => element("map-view")?.state.zoom, z => z === zoomBefore + 1, "map wheel zoom", 2000); break; }
            catch (error) { inputRetries.push(`map zoom:${attempt}`); if (attempt === 3) throw error; }
        }
        await observeNativeFrame(native, f => f.elements.some(n => n.type === "map-view" && (n.resources?.loadedTextures ?? 0) > 0
            && n.state.zoom === zoomBefore + 1 && n.state.markerCount === 1 && n.state.overlayCount === 1), "zoom retains map overlays");
        await capture("map-zoomed");
        await visit("Sky View", 445);
        await observeNativeFrame(native, f => f.elements.some(n => n.type === "di-js-canvas" && n.bounds[2] > 0 && n.resources?.scriptReady), "actual sky script visible");
        await capture("sky");
        await stream;
        const streamElapsedMs = performance.now() - start;
        await waitFor(() => { inspect(); return observations; }, o => o.sat.has(1200) && o.pvt.has(1200), "final telemetry in submitted state");
        if (observationError) throw observationError;
        check(receipts.sat.size === 1201 && receipts.pvt.size === 1201 && messageCount === 2402, "Sustained input accounting failed");
        check(delivery.length === 1200 && delivery.at(-1)! >= 59990, "Sustained scheduling was shorter than 60 seconds");
        await delay(Math.max(0, start + 60000 - performance.now()));
        const streamAfter = snapshot();
        save("stream", { requestedHzPerType: 20, requestedPerType: 1200, delivery, receipts: { sat: [...receipts.sat], pvt: [...receipts.pvt] },
            observations: { sat: [...observations.sat.values()], pvt: [...observations.pvt.values()] }, highWater, operations });
        await capture("sky-final");
        // Connected pause preserves stale seconds while static canvas activity stops.
        await waitFor(() => hasText(/stale (?:[4-9]|[1-9][0-9]+)s/), Boolean, "connected stale status", 10000);
        const pauseStart = snapshot();
        const staleBefore = [...nodes.values()].find(p => /stale \d+s/.test(p.text ?? ""))?.text;
        await delay(2200);
        check([...nodes.values()].some(p => /stale \d+s/.test(p.text ?? "") && p.text !== staleBefore), "Connected stale seconds stopped updating");
        const pauseEnd = snapshot();
        check(pauseEnd.scheduler.activeOwners === 0 && counterDelta(pauseEnd.scheduler.submitted, pauseStart.scheduler.submitted) < 20,
            "Data-driven sky retained continuous frames during connected pause");
        await capture("paused-stale");
        const skySize = element("di-js-canvas")!.bounds[2];
        native.resizeWindow(1180, 820);
        await observeNativeFrame(native, f => f.elements.some(n => n.type === "di-js-canvas" && n.bounds[2] !== skySize), "static sky resize wake");
        await capture("sky-resized");
        native.resizeWindow(1280, 900);
        send(1201);
        await waitFor(() => { inspect(); return observations; }, o => o.sat.has(1201) && o.pvt.has(1201), "resumed input submitted");
        await capture("resumed");
        await visit("Connection", 46);
        await clickUntil("Disconnect callback", 44, 140, () => serialManager.getStatus() === "disconnected");
        check(ports[0].eventNames().length === 0 && parsers[0].eventNames().length === 0, "Disconnected transport/parser retained listeners");
        await connect();
        send(1202);
        await waitFor(() => { inspect(); return observations; }, o => o.sat.has(1202) && o.pvt.has(1202), "reconnected telemetry submitted");
        await clickUntil("Final disconnect callback", 44, 140, () => serialManager.getStatus() === "disconnected");
        await visit("Sky View", 445);
        await input("move", 1100, 875);
        await delay(4500);
        const idle = await waitForNativeIdle(native, "disconnected App settled");
        await delay(10000);
        const afterIdle = snapshot();
        check(afterIdle.scheduler.submitted === idle.scheduler.submitted && afterIdle.scheduler.constructed === idle.scheduler.constructed,
            "Disconnected App constructed/submitted continuing frames");
        await capture("disconnected");
        // Real resource work on the actual static sky canvas still wakes it.
        const canvasId = element("di-js-canvas")!.id;
        const canvasPublicId = service.getDiagnostics().mappings.find((mapping: any) => mapping.nativeId === canvasId)?.publicId;
        check(canvasPublicId, "Canvas public registration missing");
        service.loadCanvasTexture(canvasPublicId, "qualification", resolve(resources.assets, "fixture.png"));
        await observeNativeFrame(native, f => f.elements.some(n => n.id === canvasId && n.resources?.loadedTextures === 1), "static sky resource upload wake");
        // Queue new Map HTTP work and release it only after the ordinary disposer.
        await visit("Map", 330);
        await waitFor(async () => (await fetch(`${resources.controlUrl}/state?group=app`)).json(), s => s.pending === 0, "map requests settle before late fixture");
        await fetch(`${resources.controlUrl}/reset?group=app`);
        await input("move", 640, 450); await delay(200); await input("wheel", 640, 450);
        const held = await waitFor(async () => (await fetch(`${resources.controlUrl}/state?group=app`)).json(), s => s.pending > 0, "real late Map request held");
        clearInterval(poll); inspect();
        if (observationError) throw observationError;
        const populated = { native: snapshot(), semantic: semantic() };
        await entry.dispose();
        serialManager.off("NAV-SAT", onSat); serialManager.off("NAV-PVT", onPvt);
        serialManager.off("message", onMessage); serialManager.off("rawdata", onRaw);
        await waitFor(() => serialManager.eventNames().length, (n: number) => n === 0, "all application passive subscriptions removed");
        check(appIntervals.size === 0, "Application retained interval handles after passive cleanup");
        const empty = await observeNativeFrame(native, f => f.elementCount === 0 && f.hierarchyCount === 1 && f.internalSubjectCount === 0, "ordinary disposer empty native baseline");
        check(ports.every(p => !p.isOpen && p.eventNames().length === 0) && parsers.every(p => p.eventNames().length === 0), "Port/parser lifetime leak");
        const generation = empty.scheduler.generation;
        for (const port of ports) { port.emit("data", Buffer.concat([navSatPacket(1203), navPvtPacket(1203)])); port.emit("close"); }
        await fetch(`${resources.controlUrl}/release?group=app`);
        await waitFor(() => snapshot(), f => f.resourceState.mapWorkers?.active === 0 && f.resourceState.mapWorkers?.queued === 0,
            "retired map workers finish late resources");
        const final = await waitForNativeIdle(native, "disposed App settles");
        check(final.scheduler.generation === generation, "Late transport/resource input revived the disposed App");
        check(final.scheduler.ownerCount === baseline.scheduler.ownerCount && final.scheduler.activeOwners === 0 && final.scheduler.deadlines === 0,
            "App retained scheduler owners after disposal");
        check(final.resourceState.textures.liveTextures === 0 && final.resourceState.textures.retiredTextures === 0
            && final.resourceState.queuedPrefetchEvents === 0, "App retained native resources");
        const js = manager.getDiagnostics(), registrations = service.getDiagnostics();
        check(js.fiberCount === 0 && js.committedDescriptionCount === 0 && js.pendingEventCount === 0 && js.subscriptionClosed,
            "App retained Fabric descriptions/events");
        check(["mappingCount", "tableCount", "mapCount", "reverseMappingCount", "nativeCount", "registrationCount"]
            .every(key => registrations[key] === 0) && registrations.disposed, "App retained JS registrations");
        check(Number(messageCount) === 2406 && Number(receipts.sat.size) === 1203 && Number(receipts.pvt.size) === 1203, "Final message accounting differs");
        const distribution = (kind: "sat" | "pvt") => {
            const samples = [...observations[kind].values()].filter(s => s.sequence >= 1 && s.sequence <= 1200).map(s => s.latencyMs).sort((a, b) => a - b);
            const p = (fraction: number) => samples[Math.ceil(samples.length * fraction) - 1];
            return { observed: samples.length, coalescedOrUnobservedBetweenPolls: 1200 - samples.length,
                p50: p(.5), p95: p(.95), p99: p(.99), maximum: p(1) };
        };
        finalReport = { status: "awaiting-native-shutdown", mode: "production", app, metadata: { node: process.version,
            react: requireApp("react").version, os: `${platform()} ${release()}`, cpu: cpus()[0]?.model, backend: (initial as any).backend },
            input: { requestedPerType: 1200, requestedHzPerType: 20, scheduledDurationMs: 60000, deliveredElapsedMs: delivery.at(-1),
                streamElapsedMs, actualHzPerType: 1200000 / delivery.at(-1)!, decodedPerTypeIncludingLifecycle: receipts.sat.size,
                messageCount, rawCallbacks, rawBytes, splitPairs, coalescedPairs },
            receiptToObservedSubmissionMs: { sat: distribution("sat"), pvt: distribution("pvt") },
            frameCounts: { sustained: counterDelta(streamAfter.scheduler.submitted, streamBefore.scheduler.submitted),
                connectedPause: counterDelta(pauseEnd.scheduler.submitted, pauseStart.scheduler.submitted), disconnected10s: 0 },
            highWater, operations, inputRetries, heldRequestsAtUnmount: held, populated, baseline, final, js, registrations,
            applicationIntervalsAfterCleanup: appIntervals.size,
            portsAfterCleanup: ports.map(p => ({ open: p.isOpen, listeners: p.eventNames().length })),
            parserListenersAfterCleanup: parsers.map(p => p.eventNames().length), telemetryListenersAfterCleanup: serialManager.eventNames().length,
            processMeasurements: { rssBefore: memoryBefore, rssAfter: process.memoryUsage().rss, cpuMicroseconds: process.cpuUsage(cpuBefore) },
            unavailable: ["physical serial", "network tile service", "GPU completion/presentation latency", "hardware WebGPU", "controlled performance"] };
        save("result", finalReport);
        await resources.close();
        await input("close");
        return;
    }
    await capture("startup");
    const before = readDiagnostics(native);
    await delay(3500);
    save("disconnected-before-fixes", { before, after: readDiagnostics(native), semantic: semantic() });
    await input("click", 44, 140);
    await waitFor(() => serialManager.getStatus(), (s: string) => s === "connected", "Connect button event");
    ports.at(-1)!.emit("data", Buffer.concat([navSatPacket(1), navPvtPacket(1)]));
    await delay(500);
    await visit("Signals", 505);
    await delay(500);
    await capture("signals");
    await visit("Sky View", 445);
    await delay(500);
    await capture("sky");
    const skyBefore = readDiagnostics(native);
    await delay(1500);
    save("sky-before-fixes", { before: skyBefore, after: readDiagnostics(native), semantic: semantic() });
    await visit("Map", 330);
    await delay(1000);
    await capture("map");
    await visit("Messages", 188);
    await delay(500);
    await capture("messages");
    serialManager.disconnect();
    // Probe remains a bounded development startup check, never sustained acceptance.
    await entry.dispose();
    await waitFor(() => serialManager.eventNames().length, (n: number) => n === 0, "App passive subscriptions released");
    await observeNativeFrame(native, f => f.elementCount === 0, "ordinary disposer native empty tree");
    save("result", { status: "probe-passed", initial, final: readDiagnostics(native), semantic: semantic(),
        framesDuringDisconnectedProbe: counterDelta(readDiagnostics(native).scheduler.submitted, before.scheduler.submitted) });
    await resources.close();
}
const watchdog = setTimeout(() => { console.error("Application watchdog expired"); process.exit(1); }, 180_000);
main().then(() => { clearTimeout(watchdog); process.exit(0); }).catch(error => {
    save("failure", { error: String(error), stack: error?.stack, native: native ? readDiagnostics(native) : null });
    console.error(error); clearTimeout(watchdog); process.exit(1);
});
