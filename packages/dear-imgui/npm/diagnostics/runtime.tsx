import React from "react";
import { createBridge, type NativeBinding } from "./bridge";
import { check, knownFailure, waitFor } from "./assertions";
import { Fixture, makeHandles, makeRows } from "./fixture";

export type NativeFrame = { enabled: boolean; frame: number; sampledAtMs: number; constructedAtMs: number; submittedAtMs: number;
    elementCount: number; hierarchyCount: number; internalSubjectCount: number; unreachableCount: number; vertices: number;
    elements: { id: number; type: string; children: number[]; yogaChildren: number[]; yogaParent: number | null;
        lastInternalOpMs: number | null; bounds: number[]; state?: any }[] };
export type RunOptions = { rows: number; points: number; rates: number[]; durationMs: number; warmupMs: number; idleMs: number; cycles: number; repetitions: number };
export const defaults: RunOptions = { rows: 1000, points: 128, rates: [20, 60, 120], durationMs: 1000, warmupMs: 200, idleMs: 500, cycles: 3, repetitions: 1 };
export function validateOptions(options: RunOptions) {
    for (const name of ["rows", "points", "durationMs", "idleMs", "repetitions"] as const)
        check(Number.isInteger(options[name]) && options[name] > 0, `${name} must be a positive integer`);
    for (const name of ["cycles", "warmupMs"] as const)
        check(Number.isInteger(options[name]) && options[name] >= 0, `${name} must be a nonnegative integer`);
    check(Array.isArray(options.rates) && options.rates.length > 0 && options.rates.every(rate => Number.isFinite(rate) && rate > 0), "rates must contain positive finite numbers");
    check(options.cycles <= 1000 && options.rows <= 100000 && options.points <= 100000, "Bounded fixture limits: 1000 cycles, 100000 rows/points");
}
export const delay = (ms: number) => new Promise<void>(resolve => setTimeout(resolve, Math.max(0, ms)));
export function distribution(values: number[]) {
    const sorted = values.filter(Number.isFinite).sort((a, b) => a - b);
    const percentile = (p: number) => sorted.length ? sorted[Math.max(0, Math.ceil(p * sorted.length) - 1)] : null;
    return { samples: sorted.length, p50: percentile(0.5), p95: percentile(0.95), p99: percentile(0.99), maximum: percentile(1) };
}

export async function runRuntime(binding: NativeBinding, options: RunOptions,
    hooks: { capture: () => Promise<void>; report: (value: any) => void; resources: () => any; metadata: any }) {
    validateOptions(options);
    const runtimeStartMs = performance.now();
    const report: any = { schemaVersion: 1, status: "running", options, metadata: hooks.metadata, stages: [], streams: [], knownFailures: [],
        timingDefinitions: { dataToObservedFrameMs: "JS performance.now: imperative invocation to poll observing a submitted frame containing that sample; includes polling delay",
            applyToConstructedMs: "C++ steady_clock: internal-operation handler completion to construction of a frame containing that state",
            submittedAtMs: "C++ steady_clock after backend draw submission; no display/presentation timestamp",
            clockPolicy: "Durations use one clock each; JS and C++ absolute timestamps are never subtracted" },
        limitations: ["Opt-in native snapshots and bridge tracing add overhead", "Only the final sample per series per frame is observable; earlier samples are coalesced", "Screenshots supplement semantic assertions", "No physical byte-arrival or presentation measurement"] };
    const bridge = createBridge(binding);
    const read = () => JSON.parse(binding.getDiagnostics()) as NativeFrame;
    let lastFrame: NativeFrame | undefined;
    let expectedOrphans = 0;
    const publish = () => { report.bridge = bridge.snapshot(); report.lastFrame = lastFrame; hooks.report(report); };
    const observe = async (predicate: (frame: NativeFrame) => boolean, label: string, after = lastFrame?.frame ?? -1) => {
        lastFrame = await waitFor(read, frame => frame.enabled && frame.frame > after && predicate(frame)
            && frame.unreachableCount === expectedOrphans, label);
        check([lastFrame.constructedAtMs, lastFrame.submittedAtMs, lastFrame.sampledAtMs].every(Number.isFinite)
            && lastFrame.constructedAtMs <= lastFrame.submittedAtMs && lastFrame.submittedAtMs <= lastFrame.sampledAtMs,
            "Invalid completed-frame timestamps");
        check(lastFrame.elements.length === lastFrame.elementCount, "Fixture snapshot was truncated or has missing element records");
        check(!lastFrame.elements.some(node => node.yogaChildren.some(id => id === null)), "Unknown Yoga child in frame");
        const nodes = new Map(lastFrame.elements.map(node => [node.id, node]));
        for (const node of nodes.values()) {
            check(JSON.stringify(node.children) === JSON.stringify(node.yogaChildren), `Hierarchy/Yoga children disagree for ${node.id}`);
            for (const id of node.children) check(nodes.get(id)?.yogaParent === node.id, `Child ${id} has the wrong Yoga owner`);
        }
        check(lastFrame.unreachableCount === expectedOrphans, `Unreachable-node signature changed: expected ${expectedOrphans}, got ${lastFrame.unreachableCount}`);
        return lastFrame;
    };
    const stage = (name: string) => { report.stages.push({ name, elapsedMs: performance.now() - runtimeStartMs, frame: lastFrame, js: bridge.snapshot().bridge,
        registrations: bridge.registrations.getDiagnostics(), resources: hooks.resources() }); publish(); };
    try {
        binding.setDiagnosticsEnabled(true);
        await observe(frame => frame.elementCount === 0, "empty native runtime");
        stage("initial");
        let handles = makeHandles();
        const mount = async () => {
            handles = makeHandles();
            await bridge.render(React.createElement(Fixture, { handles, points: options.points }));
            await waitFor(() => handles.plot.current !== null && handles.table.current !== null, Boolean, "imperative refs");
            handles.table.current!.setTableData(makeRows(options.rows));
            handles.plot.current!.setSeriesData([30, 35].map(y => ({ data: Array.from({ length: Math.min(options.points, 8) }, (_, i) => ({ x: i - Math.min(options.points, 8) + 1, y: y - (Math.min(options.points, 8) - i - 1) % 4 })) })));
            await observe(frame => frame.elements.some(node => node.type === "di-table" && node.state.rowCount === options.rows
                && node.state.columnCount === 3 && Number(node.state.firstRow.sequence) === 0
                && Number(node.state.firstRow.signal) === 20 && node.state.firstRow.used === "true")
                && frame.elements.some(node => node.type === "plot-bar" && node.state.series[1].lastY === 35), "populated plot and table");
        };
        await mount();
        stage("mounted");
        const stationMapping = bridge.registrations.getDiagnostics().mappings;
        const stationParent = stationMapping.find(item => item.publicId === "stations")!.nativeId;
        const originalChildren = lastFrame!.elements.find(node => node.id === stationParent)!.children;
        await bridge.render(React.createElement(Fixture, { handles, points: options.points, reversed: true }));
        await observe(frame => JSON.stringify(frame.elements.find(node => node.id === stationParent)?.children) === JSON.stringify([...originalChildren].reverse()), "keyed reorder");
        stage("reordered");
        check(lastFrame!.vertices > 0, "Fixture produced no draw vertices");
        await hooks.capture();
        report.screenshotCaptured = true;

        const idleBefore = read();
        const resourceBefore = hooks.resources();
        const idleStart = performance.now();
        await delay(options.idleMs);
        const idleAfter = read();
        report.idle = { elapsedMs: performance.now() - idleStart, frames: idleAfter.frame - idleBefore.frame,
            before: resourceBefore, after: hooks.resources() };
        const cpuBefore = report.idle.before.cpuMicroseconds, cpuAfter = report.idle.after.cpuMicroseconds;
        report.idle.cpuPercentOfOneCore = cpuBefore && cpuAfter
            ? ((cpuAfter.user - cpuBefore.user + cpuAfter.system - cpuBefore.system) / 1000) / report.idle.elapsedMs * 100 : null;
        publish();

        let sequence = options.rows;
        let expectedRows = options.rows;
        for (let repetition = 0; repetition < options.repetitions; repetition++) {
          for (const rate of options.rates) {
            // Warm-up uses the same data path and is excluded from measured samples.
            for (let i = 0; i < Math.ceil(rate * options.warmupMs / 1000); i++) {
                handles.plot.current!.appendSeriesData(0, sequence++, 25 + i % 20);
                handles.plot.current!.appendSeriesData(1, sequence - 1, 30 + i % 20);
                handles.table.current!.appendDataToTable([{ sequence: sequence - 1, signal: i % 40, used: i % 2 === 0 }]);
                expectedRows++;
                await delay(1000 / rate);
            }
            const start = performance.now();
            const startFrame = read().frame;
            const beforeOperations = bridge.observer.snapshot();
            const beforeResources = hooks.resources();
            const sent = new Map<number, number>();
            const observed = new Set<number>();
            const coalesced = new Set<number>();
            const latency: number[] = [], nativeLatency: number[] = [];
            const inputCount = Math.max(1, Math.floor(rate * options.durationMs / 1000));
            let endProduction = false;
            let pollError: unknown;
            const poll = (async () => {
                let frameNumber = startFrame;
                while (!endProduction || observed.size + coalesced.size < sent.size) {
                    const frame = read();
                    if (frame.frame > frameNumber) {
                        frameNumber = frame.frame;
                        lastFrame = frame;
                        const plot = frame.elements.find(node => node.type === "plot-bar");
                        const sample = plot?.state.series[0].lastX as number | undefined;
                        if (sample !== undefined && sent.has(sample) && !observed.has(sample)) {
                            observed.add(sample);
                            latency.push(performance.now() - sent.get(sample)!);
                            if (plot!.lastInternalOpMs !== null) nativeLatency.push(frame.constructedAtMs - plot!.lastInternalOpMs);
                            for (const prior of sent.keys()) if (prior < sample && !observed.has(prior)) coalesced.add(prior);
                        }
                    }
                    if (performance.now() - start > options.durationMs + 10_000) throw new Error("Final streaming sample never reached a frame");
                    await delay(2);
                }
            })().catch(error => { pollError = error; });
            let produced = 0;
            for (; produced < inputCount; produced++) {
                await delay(start + produced * 1000 / rate - performance.now());
                if (pollError) break;
                const sample = sequence++;
                sent.set(sample, performance.now());
                handles.plot.current!.appendSeriesData(0, sample, 20 + produced % 40);
                handles.plot.current!.appendSeriesData(1, sample, 25 + produced % 35);
                handles.table.current!.appendDataToTable([{ sequence: sample, signal: produced % 40, used: produced % 2 === 0 }]);
                expectedRows++;
            }
            const productionMs = performance.now() - start;
            endProduction = true;
            await poll;
            const afterOperations = bridge.observer.snapshot();
            report.streams.push({ repetition, requestedHz: rate, produced, productionMs, observationMs: performance.now() - start,
                achievedHz: produced > 1 ? (produced - 1) * 1000 / productionMs : null,
                observedUpdates: observed.size, coalescedUpdates: coalesced.size, timedOutUpdates: sent.size - observed.size - coalesced.size,
                frames: read().frame - startFrame, dataToObservedFrameMs: distribution(latency),
                applyToConstructedMs: distribution(nativeLatency), serializedBytes: afterOperations.serializedBytes - beforeOperations.serializedBytes,
                operationCounts: Object.fromEntries(Object.entries(afterOperations.counts).map(([method, count]) => [method, count - (beforeOperations.counts[method] ?? 0)])),
                beforeResources, afterResources: hooks.resources() });
            publish();
            if (pollError) throw pollError;
            await observe(frame => frame.elements.some(node => node.type === "di-table" && node.state.rowCount === expectedRows
                && Number(node.state.lastRow.sequence) === sequence - 1 && Number(node.state.lastRow.signal) === (produced - 1) % 40
                && node.state.lastRow.used === String((produced - 1) % 2 === 0))
                && frame.elements.some(node => node.type === "plot-bar" && node.state.series.length === 2
                    && node.state.series.every((series: any) => series.lastX === sequence - 1 && series.count > 0 && series.count <= options.points)
                    && node.state.series[0].lastY === 20 + (produced - 1) % 40 && node.state.series[1].lastY === 25 + (produced - 1) % 35), "final streamed table and both plot series", startFrame);
            check(latency.length > 0 && nativeLatency.length === latency.length && latency.every(value => Number.isFinite(value) && value >= 0)
                && nativeLatency.every(value => Number.isFinite(value) && value >= 0), "Invalid streaming timings");
            check(observed.size + coalesced.size === sent.size, "Unaccounted streaming updates");
          }
        }
        await bridge.render(React.createElement(Fixture, { handles, points: options.points, visible: false }));
        await observe(frame => frame.internalSubjectCount === 0 && !frame.elements.some(node => node.type === "di-table"), "subtree destruction");
        stage("subtree-removed");
        await bridge.render(null);
        expectedOrphans += 2; // XF-LIFE-010: container 0 has no element, so its root/title survive.
        await observe(frame => frame.elementCount === expectedOrphans && frame.internalSubjectCount === 0, "root unmount characterization");
        stage("unmounted");
        const stressBaseline = { elements: lastFrame!.elementCount, subjects: lastFrame!.internalSubjectCount,
            fibers: bridge.manager.getDiagnostics().fiberCount, registrations: bridge.registrations.getDiagnostics().tableCount };
        for (let cycle = 0; cycle < options.cycles; cycle++) {
            await mount();
            await bridge.render(React.createElement(Fixture, { handles, points: options.points, reversed: true }));
            await bridge.render(React.createElement(Fixture, { handles, points: options.points, visible: false }));
            await observe(frame => frame.internalSubjectCount === 0, `stress subtree removal ${cycle}`);
            await bridge.render(null);
            expectedOrphans += 2;
            await observe(frame => frame.elementCount === expectedOrphans && frame.internalSubjectCount === 0, `stress cycle ${cycle}`);
            if ((cycle + 1) % 100 === 0 || cycle + 1 === options.cycles) {
                report.stress = { completedCycles: cycle + 1, native: { elements: lastFrame!.elementCount, subjects: lastFrame!.internalSubjectCount },
                    js: bridge.manager.getDiagnostics(), registrations: bridge.registrations.getDiagnostics(), resources: hooks.resources() };
                report.stress.baseline = stressBaseline;
                report.stress.deltas = { elements: lastFrame!.elementCount - stressBaseline.elements,
                    subjects: lastFrame!.internalSubjectCount - stressBaseline.subjects,
                    fibers: report.stress.js.fiberCount - stressBaseline.fibers,
                    registrations: report.stress.registrations.tableCount - stressBaseline.registrations };
                console.log(`Lifecycle stress: ${cycle + 1}/${options.cycles}; native=${lastFrame!.elementCount}, subjects=${lastFrame!.internalSubjectCount}`);
                publish();
            }
        }
        const final = bridge.snapshot();
        report.knownFailures.push(knownFailure("container unmount destroys native roots", lastFrame!.elementCount === 0, "XF-LIFE-010",
            lastFrame!.elementCount === 2 * (options.cycles + 1) && lastFrame!.unreachableCount === lastFrame!.elementCount && lastFrame!.internalSubjectCount === 0,
            { retainedRootsAndTitles: lastFrame!.elementCount, completedUnmounts: options.cycles + 1 }));
        report.knownFailures.push(knownFailure("unmount releases Fiber mappings", final.bridge.fiberCount === 0, "XF-LIFE-001",
            final.bridge.fiberCount === final.operations.counts.setElement && final.bridge.fiberCount > 0,
            { retained: final.bridge.fiberCount, created: final.operations.counts.setElement }));
        report.knownFailures.push(knownFailure("unmount releases widget registrations", final.registrations.tableCount === 0, "XF-LIFE-002",
            final.registrations.tableCount === 2 * (options.cycles + 1), { tableRegistrations: final.registrations.tableCount }));
        report.status = "passed-with-known-defects";
    } catch (error) {
        report.status = "failed";
        report.error = String(error);
        throw error;
    } finally {
        lastFrame = read();
        publish();
        bridge.dispose();
        binding.setDiagnosticsEnabled(false);
    }
    return report;
}
