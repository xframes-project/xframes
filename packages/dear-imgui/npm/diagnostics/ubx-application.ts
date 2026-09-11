import { createRequire } from "node:module";
import { createHash } from "node:crypto";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { EventEmitter } from "node:events";
import { cpus, platform, release } from "node:os";
import { check, waitFor } from "./assertions";
import { observeNativeFrame, readDiagnostics, counterDelta, waitForNativeIdle } from "./frames";
import { navSatPacket, navPvtPacket } from "./ubx-bytes";
import { deliverScheduledUbx } from "./ubx-source";
import { UbxHistoryLedger, UbxConsoleOracle, verifyUbxHistories } from "./ubx-history";

const argumentsByName = new Map<string, string>();
for (const argument of process.argv.slice(2)) {
    if (["--scenario", "--rate-changes", "--check-histories"].includes(argument)) continue;
    const match = /^--(receiver-hz|ui-hz|comparison)=(.+)$/.exec(argument);
    check(match, `Unknown argument: ${argument}`);
    check(!argumentsByName.has(match[1]), `Duplicate argument: ${match[1]}`);
    argumentsByName.set(match[1], match[2]);
}
const receiverHz = Number(argumentsByName.get("receiver-hz") ?? 20);
const uiHz = Number(argumentsByName.get("ui-hz") ?? 20);
check([20, 120].includes(receiverHz), "receiver-hz must be 20 or 120 for this bounded fixture");
check([10, 20, 60].includes(uiHz), "ui-hz must be 10, 20 or 60");
const packetCount = receiverHz * 60;
const rateChanges = process.argv.includes("--rate-changes");
const comparison = argumentsByName.get("comparison");
check(!comparison || ["paced", "unpaced", "qualified"].includes(comparison), "comparison must be paced, unpaced or qualified");
check(!comparison || (receiverHz === 120 && uiHz === 20 && !rateChanges), "comparison requires receiver 120, UI 20 and no rate changes");
const checkHistories = process.argv.includes("--check-histories") || rateChanges || comparison === "paced" || comparison === "unpaced";
check(!rateChanges || (receiverHz === 120 && uiHz === 20), "rate-changes requires receiver 120 Hz and initial UI 20 Hz");

const execute = promisify(execFile);
const here = __dirname;
const output = resolve(process.env.XFRAMES_DIAGNOSTICS_DIR ?? "build/diagnostics/ubx-application/probe");
mkdirSync(output, { recursive: true });
const save = (name: string, data: unknown) => writeFileSync(resolve(output, `${name}.json`), JSON.stringify(data, null, 2));
const delay = (ms: number) => new Promise<void>(done => setTimeout(done, ms));
let native: any;
let finalReport: any;
let nativeShutdownVerified = false;

async function main() {
    check(process.env.XFRAMES_UBX_APP_DIR, "Set XFRAMES_UBX_APP_DIR to the isolated application");
    const app = resolve(process.env.XFRAMES_UBX_APP_DIR);
    const requireApp = createRequire(resolve(app, "package.json"));
    const hash = (path: string) => createHash("sha256").update(readFileSync(path)).digest("hex");
    const sourceHashes: Record<string, string> = {};
    const collect = (directory: string, relative = "src") => {
        for (const entry of readdirSync(directory, { withFileTypes: true })) {
            const key = `${relative}/${entry.name}`, path = resolve(directory, entry.name);
            if (entry.isDirectory()) collect(path, key); else sourceHashes[key] = hash(path);
        }
    };
    collect(resolve(app, "src"));
    const git = async (args: string[], cwd: string) => (await execute("git", args,
        { cwd, windowsHide: true, maxBuffer: 32 * 1024 * 1024 })).stdout;
    writeFileSync(resolve(output, "application.diff"), await git(["diff", "--binary", "HEAD"], app));
    writeFileSync(resolve(output, "xframes.diff"), await git(["diff", "--binary", "HEAD"], resolve(here, "../../../..")));
    save("source-identity", { appRevision: (await git(["rev-parse", "HEAD"], app)).trim(),
        xframesRevision: (await git(["rev-parse", "HEAD"], resolve(here, "../../../.."))).trim(), sourceHashes,
        files: Object.fromEntries(["package.json", "package-lock.json", "assets/fonts/roboto-regular.ttf", "assets/fonts/roboto-mono.ttf"]
            .map(file => [file, hash(resolve(app, file))])),
        nativeSha256: hash(requireApp.resolve("@xframes/node/dist/xframes.node")),
        commonEntrySha256: hash(requireApp.resolve("@xframes/common")),
        diagnosticHashes: Object.fromEntries(["ubx-application.ts", "ubx-source.ts", "ubx-source.test.ts", "ubx-history.ts",
            "ubx-bytes.ts", "ubx-rate-select.ps1", "native-window.ps1", "frames.ts", "resource-server.mjs"]
            .map(file => [file, hash(resolve(here, file))])) });
    const { startResourceServer } = await import("./resource-server.mjs");
    const resources = await startResourceServer(resolve(here, ".."), output);
    process.env.UBX_MONITOR_TILE_URL = `${resources.baseUrl}/asset?group=app&hold=1&z={z}&x={x}&y={y}`;
    process.env.UBX_MONITOR_TILE_CACHE = resolve(output, "tile-cache");
    await fetch(`${resources.controlUrl}/release?group=app`);
    process.chdir(app);
    // This is an isolated checkout: each run starts with deterministic UI choices
    // and never imports the original receiver's machine-local configuration.
    writeFileSync(resolve(app, "config.json"), JSON.stringify({ uiUpdateRate: uiHz }) + "\n");
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
                nativeShutdownVerified = true;
                console.log("Ordinary ubx-monitor application: passed");
            } catch (error) { save("failure", { error: String(error), native: readDiagnostics(native) }); process.exit(1); }
        }
        options.onBeforeExit();
    } });
    const observerCosts = { stageMs: 0, widgetMs: 0, commitNativeMs: 0, internalNativeMs: 0,
        consoleNativeMs: 0, sourcePreparationMs: 0, pollMs: 0, polls: 0 };
    const nodes = new Map<number, any>();
    const ledger = new UbxHistoryLedger();
    const consoleOracle = new UbxConsoleOracle();
    const operations: Record<string, number> = {};
    const latest = new Map<number, any>();
    const highWater: Record<string, number> = {};
    const inputRetries: string[] = [];
    const apply = native.applyCommit;
    native.applyCommit = (wire: string) => {
        const nativeStart = performance.now();
        const result = apply(wire);
        observerCosts.commitNativeMs += performance.now() - nativeStart;
        const observerStart = performance.now();
        const ack = JSON.parse(result);
        check(ack.status === "applied", `Application publication failed: ${result}`);
        for (const op of JSON.parse(wire).operations) {
            if (op.op === "create") nodes.set(op.id, { type: op.elementType, ...op.props });
            if (op.op === "patch") Object.assign(nodes.get(op.id), op.props);
        }
        for (const id of ack.destroyedIds) { nodes.delete(id); latest.delete(id); ledger.destroy(id); }
        observerCosts.widgetMs += performance.now() - observerStart;
        return result;
    };
    const internal = native.elementInternalOp;
    native.elementInternalOp = (id: number, wire: string) => {
        const nativeStart = performance.now();
        const result = internal(id, wire);
        observerCosts.internalNativeMs += performance.now() - nativeStart;
        const observerStart = performance.now();
        const op = JSON.parse(wire);
        const key = `${nodes.get(id)?.type}:${op.op}`;
        operations[key] = (operations[key] ?? 0) + 1;
        latest.set(id, { ...latest.get(id), [op.op]: op });
        ledger.operation(id, nodes.get(id)?.type, op);
        observerCosts.widgetMs += performance.now() - observerStart;
        return result;
    };
    const appendConsole = native.appendTextToClippedMultiLineTextRenderer;
    native.appendTextToClippedMultiLineTextRenderer = (id: number, text: string) => {
        const nativeStart = performance.now();
        const result = appendConsole(id, text);
        observerCosts.consoleNativeMs += performance.now() - nativeStart;
        const observerStart = performance.now();
        ledger.appendConsole(id, text);
        observerCosts.widgetMs += performance.now() - observerStart;
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
    const presentationPath = resolve(app, "src/telemetry/presentation.ts");
    const presentation = existsSync(presentationPath)
        ? (await import(pathToFileURL(presentationPath).href)).presentation : null;
    if (comparison === "unpaced") {
        check(presentation, "Unpaced control requires the retention-correct application");
        presentation.setUnpacedForComparison(true);
    }
    if (comparison === "paced") check(presentation, "Paced comparison requires the candidate application");
    if (comparison === "qualified") check(!presentation, "Qualified characterization must use the e7de9e2 application");
    const publicationStages: any[] = [];
    let droppedStageObservations = 0;
    if (presentation) presentation.observe((event: any) => {
        const observedAt = performance.now();
        if (publicationStages.length < 250000) publicationStages.push(event);
        else droppedStageObservations++;
        observerCosts.stageMs += performance.now() - observedAt;
    });
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
            check(Number.isInteger(sequence) && sequence === receipts[kind].size && !receipts[kind].has(sequence), `Duplicate/invalid ${kind} sequence ${sequence}`);
            receipts[kind].set(sequence, performance.now());
        };
        const onSat = decoded("sat"), onPvt = decoded("pvt");
        const onMessage = () => ++messageCount;
        const onRaw = (chunk: Buffer) => { rawCallbacks++; rawBytes += chunk.length; };
        serialManager.prependListener("NAV-SAT", onSat); serialManager.prependListener("NAV-PVT", onPvt);
        serialManager.prependListener("message", onMessage); serialManager.prependListener("rawdata", onRaw);
        const listenerCounts = () => Object.fromEntries(serialManager.eventNames().map((event: string) => [event, serialManager.listenerCount(event)]));
        const subscriptions = listenerCounts();
        const parsers: any[] = [];
        const rateTransitions: any[] = [];
        const retentionChecks: any[] = [];
        const connect = async () => {
            await visit("Connection", 46);
            await clickUntil("Connect callback", 44, 140, () => serialManager.getStatus() === "connected");
            parsers.push(serialManager.parser);
            check(JSON.stringify(listenerCounts()) === JSON.stringify(subscriptions), "Connection use accumulated application subscriptions");
        };
        const send = (sequence: number) => {
            const port = ports.at(-1)!;
            check(port.isOpen, "Telemetry source lost its connection");
            const preparedAt = performance.now();
            const sat = navSatPacket(sequence), pvt = navPvtPacket(sequence);
            consoleOracle.packet(sat, "NAV-SAT"); consoleOracle.packet(pvt, "NAV-PVT");
            observerCosts.sourcePreparationMs += performance.now() - preparedAt;
            if (sequence % 2) {
                splitPairs++;
                port.emit("data", sat.subarray(0, 9));
                port.emit("data", Buffer.concat([sat.subarray(9), pvt]));
            } else { coalescedPairs++; port.emit("data", Buffer.concat([sat, pvt])); }
        };
        let observationError: unknown;
        const inspect = () => {
            const observedAt = performance.now();
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
            finally { observerCosts.pollMs += performance.now() - observedAt; observerCosts.polls++; }
        };
        const setRateThroughInput = async (rate: number, streamStart: number) => {
            check(presentation, "Rate changes require the pacing application");
            const connectionId = [...nodes].find(([, props]) => props.type === "tab-item" && props.label === "Connection")?.[0];
            if (!snapshot().elements?.some(node => node.id === connectionId && node.bounds[2] > 0))
                await visit("Connection", 46);
            const baud = serialManager.getBaudRate();
            const writes = ports.at(-1)!.writes.length;
            for (let attempt = 1; attempt <= 3; attempt++) {
                await execute("powershell.exe", ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    resolve(here, "ubx-rate-select.ps1"), "-ProcessId", String(process.pid), "-Rate", String(rate)],
                    { windowsHide: true, timeout: 10000 });
                try {
                    await waitFor(() => presentation.getDiagnostics().rate, value => value === rate, `native UI rate ${rate}`, 2000);
                    break;
                } catch (error) {
                    inputRetries.push(`UI rate ${rate}:${attempt}`);
                    await capture(`rate-${rate}-miss-${attempt}`);
                    if (attempt === 3) throw error;
                }
            }
            check(serialManager.getStatus() === "connected" && serialManager.getBaudRate() === baud
                && ports.at(-1)!.writes.length === writes, "UI rate changed receiver configuration");
            const config = (await import(pathToFileURL(resolve(app, "src/connection/config.ts")).href)).getConfig();
            check(config.uiUpdateRate === rate, "UI rate did not reach persisted configuration owner");
            const sequence = receipts.pvt.size - 1;
            check(sequence < packetCount, "Rate change occurred after scheduled input ended");
            await waitFor(() => { inspect(); return observations; }, value =>
                [...value.pvt.keys()].some(key => key >= sequence) && [...value.sat.keys()].some(key => key >= sequence),
                `submitted samples after rate ${rate}`);
            rateTransitions.push({ rate, elapsedMs: performance.now() - streamStart, sequence,
                pacing: presentation.getDiagnostics() });
            save("rate-transitions", rateTransitions);
        };
        const verifyRetention = (label: string, sequence: number, positionFirst = 0, connectionFirst = 0) => {
            const report = verifyUbxHistories(ledger, nodes, sequence, consoleOracle, positionFirst, connectionFirst);
            retentionChecks.push({ label, ...report });
            save(`retention-${label}`, { report, calls: ledger.snapshot(), native: snapshot() });
            return report;
        };
        const poll = setInterval(inspect, 10);
        await connect();
        send(0);
        if (comparison) {
            await visit("Signals", 505);
            await waitFor(() => { inspect(); return observations; }, o => o.sat.has(0) && o.pvt.has(0), "comparison warm-up submitted");
            await input("move", 1100, 875);
            await delay(4500);
            await waitFor(async () => (await fetch(`${resources.controlUrl}/state?group=app`)).json(), state => state.pending === 0,
                "comparison resources settled");
            await waitForNativeIdle(native, "comparison warm-up idle");
        }
        const streamBefore = snapshot();
        const presentationBefore = presentation?.getDiagnostics();
        const publicationsBefore = manager.getDiagnostics().appliedPublications;
        const cpuBefore = process.cpuUsage(), memoryBefore = process.memoryUsage().rss;
        const costsBefore = { ...observerCosts };
        const operationsBefore = { ...operations };
        const consoleAppendsBefore = ledger.consoleAppends;
        const start = performance.now();
        const source = deliverScheduledUbx(packetCount, receiverHz, start, send);
        const delivery = source.deliveredAt;
        const stream = source.completion;
        let measurement: any;
        void stream.then(() => {
            measurement = { mode: comparison ?? "functional", wallMs: performance.now() - start,
                cpuMicroseconds: process.cpuUsage(cpuBefore), rssBefore: memoryBefore, rssAfter: process.memoryUsage().rss,
                submittedFrames: counterDelta(snapshot().scheduler.submitted, streamBefore.scheduler.submitted),
                boundary: "source delivery completion; trailing presentation recorded separately",
                costsMs: Object.fromEntries(Object.entries(observerCosts).map(([key, value]) => [key, value - costsBefore[key as keyof typeof costsBefore]])),
                operations: Object.fromEntries(Object.entries(operations).map(([key, value]) => [key, value - (operationsBefore[key] ?? 0)])),
                consoleAppends: ledger.consoleAppends - consoleAppendsBefore,
                appliedFabricPublications: manager.getDiagnostics().appliedPublications - publicationsBefore,
                observerRecordsRetained: publicationStages.length, observerRecordLimit: 250000 };
            save("source-delivery", { deliveredAt: delivery, source: source.diagnostics(),
                deadlineLatenessMs: source.deadlineLatenessMs, actualHzPerType: packetCount * 1000 / delivery.at(-1)!, measurement });
        }).catch(() => {});
        if (!comparison) {
        if (rateChanges) {
            await setRateThroughInput(10, start);
            await setRateThroughInput(60, start);
            await setRateThroughInput(20, start);
        }
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
        }
        await stream;
        const streamElapsedMs = performance.now() - start;
        await waitFor(() => { inspect(); return observations; }, o => o.sat.has(packetCount) && o.pvt.has(packetCount), "final telemetry in submitted state");
        if (observationError) throw observationError;
        check(receipts.sat.size === (packetCount + 1) && receipts.pvt.size === (packetCount + 1) && messageCount === (2 * (packetCount + 1)), "Sustained input accounting failed");
        check(delivery.length === packetCount && delivery.at(-1)! >= 59990, "Sustained scheduling was shorter than 60 seconds");
        await delay(Math.max(0, start + 60000 - performance.now()));
        const streamAfter = snapshot();
        const presentationAfter = presentation?.getDiagnostics();
        measurement.throughFinalSample = { wallMs: performance.now() - start, cpuMicroseconds: process.cpuUsage(cpuBefore),
            rss: process.memoryUsage().rss, submittedFrames: counterDelta(streamAfter.scheduler.submitted, streamBefore.scheduler.submitted) };
        save("measurement", measurement);
        save("publication-stages", { events: publicationStages, droppedStageObservations,
            scope: "Session publication preparation and React enqueue; not React commit or native submission",
            before: presentationBefore, after: presentationAfter,
            appliedFabricPublications: manager.getDiagnostics().appliedPublications - publicationsBefore });
        save("stream", { requestedHzPerType: receiverHz, uiHz, requestedPerType: packetCount, delivery, receipts: { sat: [...receipts.sat], pvt: [...receipts.pvt] },
            observations: { sat: [...observations.sat.values()], pvt: [...observations.pvt.values()] }, highWater, operations,
            source: source.diagnostics(), deadlineLatenessMs: source.deadlineLatenessMs });
        if (comparison) {
            const expectedSequences = Array.from({ length: 3000 }, (_, i) => packetCount - 2999 + i);
            const altitudeId = [...nodes].find(([, node]) => node.type === "plot-line" && node.yAxisLabel === "Altitude (m)")?.[0];
            const actualSequences = (ledger.lines.get(altitudeId!) ?? []).map(point => Math.round(point.y * 1000 - 50000));
            save("history-characterization", { comparison, expectedSequences, actualSequences,
                exactPositionTail: JSON.stringify(expectedSequences) === JSON.stringify(actualSequences),
                missingExpected: expectedSequences.filter(sequence => !actualSequences.includes(sequence)), calls: ledger.snapshot(), native: snapshot() });
            if (checkHistories) {
                const report = verifyRetention("comparison-final", packetCount);
                await observeNativeFrame(native, frame => frame.elements.some(node => node.type === "plot-scatter"
                    && node.state?.pointCount === report.retained.positions)
                    && frame.elements.some(node => node.type === "map-view" && node.state?.polylinePoints === report.retained.trail)
                    && frame.elements.some(node => node.type === "di-table" && node.state?.rowCount === report.retained.messages)
                    && frame.elements.some(node => node.type === "clipped-multi-line-text-renderer"
                        && node.state?.byteCount === report.retained.consoleBytes), "comparison final native retention sizes");
            }
            await capture("signals-measured-final");
            const populated = snapshot();
            // Functional scenarios cover native controls, held resources and
            // long disconnected idle. Each cost repetition only needs its own
            // public teardown and terminal ownership checks after measurement.
            serialManager.disconnect();
            await entry.dispose();
            serialManager.off("NAV-SAT", onSat); serialManager.off("NAV-PVT", onPvt);
            serialManager.off("message", onMessage); serialManager.off("rawdata", onRaw);
            await waitFor(() => serialManager.eventNames().length, (count: number) => count === 0, "comparison passive cleanup");
            check(appIntervals.size === 0 && ports.every(port => !port.isOpen && !port.eventNames().length)
                && parsers.every(parser => !parser.eventNames().length), "comparison retained application/serial ownership");
            if (presentation) {
                const pacing = presentation.getDiagnostics();
                check(pacing.owners === 0 && pacing.pending === 0 && pacing.timers === 0, "comparison retained pacing work");
                presentation.observe(null);
            }
            await observeNativeFrame(native, frame => frame.elementCount === 0 && frame.internalSubjectCount === 0
                && frame.hierarchyCount === 1, "comparison public disposer empty tree");
            const final = await waitForNativeIdle(native, "comparison disposed App idle");
            check(final.scheduler.ownerCount === baseline.scheduler.ownerCount && final.scheduler.activeOwners === 0
                && final.scheduler.deadlines === 0 && final.resourceState.textures.liveTextures === 0
                && final.resourceState.textures.retiredTextures === 0, "comparison retained native ownership");
            const js = manager.getDiagnostics(), registrations = service.getDiagnostics();
            check(js.fiberCount === 0 && js.committedDescriptionCount === 0 && js.pendingEventCount === 0 && js.subscriptionClosed,
                "comparison retained Fabric ownership");
            check(["mappingCount", "tableCount", "mapCount", "reverseMappingCount", "nativeCount", "registrationCount"]
                .every(key => registrations[key] === 0) && registrations.disposed, "comparison retained registrations");
            check(receipts.sat.size === packetCount + 1 && receipts.pvt.size === packetCount + 1
                && messageCount === 2 * (packetCount + 1), "comparison source accounting changed after measurement");
            finalReport = { status: "awaiting-native-shutdown", comparison, app, measurement,
                metadata: { node: process.version, react: requireApp("react").version, os: `${platform()} ${release()}`,
                    cpu: cpus()[0]?.model, backend: (initial as any).backend, dimensions: [1280, 900],
                    observerIntervalMs: 10, resources: "fresh per-run local fixture cache; warm-up settled before measurement" },
                hostConditions: process.env.XFRAMES_UBX_HOST_CONDITIONS ?? "shared host; quiet window not confirmed",
                input: { requestedPerType: packetCount, requestedHzPerType: receiverHz, uiHz,
                    deliveredElapsedMs: delivery.at(-1), actualHzPerType: packetCount * 1000 / delivery.at(-1)!,
                    requestedRateMet: packetCount * 1000 / delivery.at(-1)! >= receiverHz * .95,
                    decodedPerTypeIncludingWarmup: receipts.sat.size, messageCount, rawCallbacks, rawBytes, splitPairs, coalescedPairs,
                    source: source.diagnostics() },
                presentation: { before: presentationBefore, after: presentationAfter, disposed: presentation?.getDiagnostics(),
                    droppedStageObservations, observationCapacity: 250000 },
                retentionChecks, highWater, operations, inputRetries, populated, baseline, final, js, registrations,
                applicationIntervalsAfterCleanup: appIntervals.size, telemetryListenersAfterCleanup: serialManager.eventNames().length,
                unavailable: ["GPU completion/presentation", "physical serial", "controlled framework performance qualification"] };
            save("result", finalReport);
            await resources.close();
            await input("close");
            await waitFor(() => nativeShutdownVerified, Boolean, "comparison native shutdown callback", 10000);
            return;
        }
        await capture("sky-final");
        if (checkHistories) {
            let retentionError: unknown;
            await waitFor(() => { try { verifyUbxHistories(ledger, nodes, packetCount, consoleOracle); return true; }
                catch (error) { retentionError = error; return false; } }, Boolean, "final ordered retained histories")
                .catch(error => { save("retention-failure", { error: String(retentionError), calls: ledger.snapshot(), native: snapshot() }); throw error; });
            verifyRetention("stream-final", packetCount);
            await visit("Position", 380);
            await observeNativeFrame(native, frame => frame.elements.some(node => node.type === "plot-scatter"
                && node.state?.pointCount === Math.min(3000, packetCount + 1)), "populated Position retained scatter");
            await capture("position-final");
            await visit("Console", 119);
            await observeNativeFrame(native, frame => frame.elements.some(node => node.type === "clipped-multi-line-text-renderer"
                && node.state?.byteCount === Buffer.byteLength(consoleOracle.text)), "final Console retained native bytes");
            await capture("console-final");
            await visit("Sky View", 445);
        }
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
        if (checkHistories) {
            await visit("Position", 380);
            await clickUntil("Position reset callback", 40, 84, () => hasText(/No data/)
                && ![...nodes.values()].some(node => node.type === "plot-scatter"));
            await capture("position-reset");
            await visit("Sky View", 445);
        }
        send((packetCount + 1));
        await waitFor(() => { inspect(); return observations; }, o => o.sat.has((packetCount + 1)) && o.pvt.has((packetCount + 1)), "resumed input submitted");
        await capture("resumed");
        if (checkHistories) verifyRetention("after-position-reset", packetCount + 1, packetCount + 1);
        await visit("Connection", 46);
        await clickUntil("Disconnect callback", 44, 140, () => serialManager.getStatus() === "disconnected");
        check(ports[0].eventNames().length === 0 && parsers[0].eventNames().length === 0, "Disconnected transport/parser retained listeners");
        await connect();
        consoleOracle.reset();
        send((packetCount + 2));
        await waitFor(() => { inspect(); return observations; }, o => o.sat.has((packetCount + 2)) && o.pvt.has((packetCount + 2)), "reconnected telemetry submitted");
        if (checkHistories) verifyRetention("reconnect", packetCount + 2, packetCount + 2, packetCount + 2);
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
        if (presentation) {
            const pacing = presentation.getDiagnostics();
            check(pacing.owners === 0 && pacing.pending === 0 && pacing.timers === 0,
                "Application retained pacing ownership after passive cleanup");
            presentation.observe(null);
        }
        const empty = await observeNativeFrame(native, f => f.elementCount === 0 && f.hierarchyCount === 1 && f.internalSubjectCount === 0, "ordinary disposer empty native baseline");
        check(ports.every(p => !p.isOpen && p.eventNames().length === 0) && parsers.every(p => p.eventNames().length === 0), "Port/parser lifetime leak");
        const generation = empty.scheduler.generation;
        for (const port of ports) { port.emit("data", Buffer.concat([navSatPacket((packetCount + 3)), navPvtPacket((packetCount + 3))])); port.emit("close"); }
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
        check(Number(messageCount) === (2 * (packetCount + 3)) && Number(receipts.sat.size) === (packetCount + 3) && Number(receipts.pvt.size) === (packetCount + 3), "Final message accounting differs");
        const distribution = (kind: "sat" | "pvt") => {
            const samples = [...observations[kind].values()].filter(s => s.sequence >= 1 && s.sequence <= packetCount).map(s => s.latencyMs).sort((a, b) => a - b);
            const p = (fraction: number) => samples[Math.ceil(samples.length * fraction) - 1];
            return { observed: samples.length, coalescedOrUnobservedBetweenPolls: packetCount - samples.length,
                p50: p(.5), p95: p(.95), p99: p(.99), maximum: p(1) };
        };
        finalReport = { status: "awaiting-native-shutdown", mode: "production", app, metadata: { node: process.version,
            react: requireApp("react").version, os: `${platform()} ${release()}`, cpu: cpus()[0]?.model, backend: (initial as any).backend },
            input: { requestedPerType: packetCount, requestedHzPerType: receiverHz, uiHz, scheduledDurationMs: 60000, deliveredElapsedMs: delivery.at(-1),
                streamElapsedMs, actualHzPerType: (packetCount * 1000) / delivery.at(-1)!, decodedPerTypeIncludingLifecycle: receipts.sat.size,
                messageCount, rawCallbacks, rawBytes, splitPairs, coalescedPairs, source: source.diagnostics(),
                requestedRateMet: (packetCount * 1000) / delivery.at(-1)! >= receiverHz * .95 },
            receiptToObservedSubmissionMs: { sat: distribution("sat"), pvt: distribution("pvt") },
            frameCounts: { sustained: counterDelta(streamAfter.scheduler.submitted, streamBefore.scheduler.submitted),
                connectedPause: counterDelta(pauseEnd.scheduler.submitted, pauseStart.scheduler.submitted), disconnected10s: 0 },
            highWater, operations, inputRetries, rateTransitions, retentionChecks,
            consoleOperations: { appends: ledger.consoleAppends, codeUnits: ledger.consoleAppendedCodeUnits }, heldRequestsAtUnmount: held, populated, baseline, final, js, registrations,
            applicationIntervalsAfterCleanup: appIntervals.size,
            measurement,
            presentation: { before: presentationBefore, after: presentationAfter, disposed: presentation?.getDiagnostics(),
                observationCapacity: 250000, droppedStageObservations },
            portsAfterCleanup: ports.map(p => ({ open: p.isOpen, listeners: p.eventNames().length })),
            parserListenersAfterCleanup: parsers.map(p => p.eventNames().length), telemetryListenersAfterCleanup: serialManager.eventNames().length,
            processMeasurements: { rssBefore: memoryBefore, rssAfter: process.memoryUsage().rss, cpuMicroseconds: process.cpuUsage(cpuBefore) },
            unavailable: ["physical serial", "network tile service", "GPU completion/presentation latency", "hardware WebGPU", "controlled performance"] };
        save("result", finalReport);
        await resources.close();
        await input("close");
        // WM_CLOSE can return before the native thread-safe shutdown callback.
        // Keep the JS loop alive for that callback; process exit 0 alone is not
        // proof of terminal native ownership. The ordinary callback exits itself.
        await waitFor(() => nativeShutdownVerified, Boolean, "ordinary native shutdown callback", 10000);
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
const watchdog = setTimeout(() => { console.error("Application watchdog expired"); process.exit(1); }, 240_000);
main().then(() => { clearTimeout(watchdog); process.exit(0); }).catch(error => {
    save("failure", { error: String(error), stack: error?.stack, native: native ? readDiagnostics(native) : null });
    console.error(error); clearTimeout(watchdog); process.exit(1);
});
