import { readFileSync } from "node:fs";

const paths = process.argv.slice(2);
if (!paths.length) throw new Error("Pass one or more diagnostic result.json paths");
const range = values => {
    if (!values.length || values.some(value => !Number.isFinite(value))) throw new Error("Missing/invalid required measurement");
    const low = Math.min(...values).toFixed(2), high = Math.max(...values).toFixed(2);
    return low === high ? low : `${low}–${high}`;
};
for (const path of paths) {
    const input = JSON.parse(readFileSync(path, "utf8"));
    const report = input.report ?? input;
    if (input.report && input.status !== "complete") throw new Error(`Incomplete browser run: ${path}`);
    if (!["passed", "passed-with-known-defects"].includes(report.status) || !report.streams?.length) throw new Error(`Incomplete diagnostic run: ${path}`);
    for (const run of report.streams) {
        if (run.timedOutUpdates !== 0 || run.observedUpdates + run.coalescedUpdates !== run.produced
            || !run.dataToObservedFrameMs.samples || !run.applyToConstructedMs.samples)
            throw new Error(`Invalid update accounting: ${path}`);
    }
    console.log(`\n${report.metadata.runtime} — ${report.options.rows} initial rows, ${report.options.repetitions} repetitions (${path})\n`);
    console.log("| Input Hz | Achieved Hz | Observed / produced | JS p50 ms | JS p95 ms | JS p99 ms | JS max ms | Native p95 ms |");
    console.log("| --- | --- | --- | --- | --- | --- | --- | --- |");
    for (const rate of report.options.rates) {
        const runs = report.streams.filter(run => run.requestedHz === rate);
        const sum = key => runs.reduce((total, run) => total + run[key], 0);
        console.log(`| ${rate} | ${range(runs.map(run => run.achievedHz))} | ${sum("observedUpdates")} / ${sum("produced")} | ${range(runs.map(run => run.dataToObservedFrameMs.p50))} | ${range(runs.map(run => run.dataToObservedFrameMs.p95))} | ${range(runs.map(run => run.dataToObservedFrameMs.p99))} | ${Math.max(...runs.map(run => run.dataToObservedFrameMs.maximum)).toFixed(2)} | ${range(runs.map(run => run.applyToConstructedMs.p95))} |`);
    }
    console.log("\nRanges are per-repetition values, not pooled percentiles. JS intervals include observation polling; native intervals are widget state age at frame construction.");
    if (report.publications) {
        if (report.publications.status !== "passed" || report.publications.results.length !== report.options.repetitions)
            throw new Error(`Incomplete Fabric structural workload: ${path}`);
        const runs = report.publications.results;
        console.log("\n| React updates | Publications / calls | Bailouts | UTF-8 bytes | Staging p95 ms | Diff p95 ms | Serialization p95 ms | Boundary p95 ms | Total publication p95 ms |");
        console.log("| --- | --- | --- | --- | --- | --- | --- | --- | --- |");
        console.log(`| ${range(runs.map(run => run.requestedUpdates))} | ${range(runs.map(run => run.publications))} | ${range(runs.map(run => run.bailouts))} | ${range(runs.map(run => run.wireBytes))} | ${range(runs.map(run => run.stagingMs.p95))} | ${range(runs.map(run => run.diffMs.p95))} | ${range(runs.map(run => run.serializationMs.p95))} | ${range(runs.map(run => run.boundaryMs.p95))} | ${range(runs.map(run => run.publicationTotalMs.p95))} |`);
        console.log(report.publications.timingScope);
    }
    if (report.transactions?.schemaVersion === 2) {
        if (report.transactions.status !== "passed") throw new Error(`Failed publication fixture: ${path}`);
        const runs = report.transactions.overhead;
        if (runs.length !== 3 || runs.some(run => run.mode !== "final-tree-publication")) throw new Error(`Missing publication overhead repetitions: ${path}`);
        const timing = key => runs.every(run => run[key].samples > 0) ? range(runs.map(run => run[key].p95 * 1000)) : "unsampled";
        console.log("\n| Patches | Child assignments | Calls | UTF-8 bytes | Boundary p95 µs | Parse p95 µs | Validation/reachability p95 µs | Application p95 µs | Lock wait p95 µs | Lock held p95 µs |");
        console.log("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |");
        console.log(`| ${range(runs.map(run => run.patches))} | ${range(runs.map(run => run.childAssignments))} | ${range(runs.map(run => run.boundaryCalls))} | ${range(runs.map(run => run.bytes))} | ${timing("boundaryMs")} | ${timing("parseMs")} | ${timing("validationMs")} | ${timing("applicationMs")} | ${timing("lockWaitMs")} | ${timing("lockHeldMs")} |`);
        console.log(report.transactions.overhead[0].timingScope);
    } else if (report.transactions) {
        // Historical Stage 2 reports remain readable for measured comparisons.
        if (report.transactions.status !== "passed") throw new Error(`Failed transaction fixture: ${path}`);
        console.log("\n| Transaction mode | Operations | Calls | UTF-8 bytes | Boundary p95 µs | Parse p95 µs | Envelope p95 µs | Validation p95 µs | Application p95 µs |");
        console.log("| --- | --- | --- | --- | --- | --- | --- | --- | --- |");
        for (const mode of ["batch", "compatibility"]) {
            const runs = report.transactions.overhead.filter(run => run.mode === mode);
            if (runs.length !== 3) throw new Error(`Missing transaction repetitions: ${path}`);
            const timing = key => runs.every(run => run[key].samples > 0) ? range(runs.map(run => run[key].p95 * 1000)) : "unsampled";
            console.log(`| ${mode} | ${range(runs.map(run => run.operations))} | ${range(runs.map(run => run.boundaryCalls))} | ${range(runs.map(run => run.bytes))} | ${timing("boundaryMs")} | ${timing("parseMs")} | ${timing("envelopeMs")} | ${timing("validationMs")} | ${timing("applicationMs")} |`);
        }
        console.log(report.transactions.overhead[0].timingScope);
    }
    console.log(JSON.stringify({ metadata: report.metadata, backend: report.lastFrame.backend,
        idle: { ms: report.idle.elapsedMs, frames: report.idle.frames, cpuPercentOfOneCore: report.idle.cpuPercentOfOneCore,
            rssBefore: report.idle.before.rssBytes, rssAfter: report.idle.after.rssBytes },
        milestones: report.stages.map(stage => ({ name: stage.name, elapsedMs: stage.elapsedMs })),
        final: { native: report.lastFrame.elementCount, subjects: report.lastFrame.internalSubjectCount,
            fibers: report.bridge.bridge.fiberCount, registrations: report.bridge.registrations.tableCount },
    }, null, 2));
}
