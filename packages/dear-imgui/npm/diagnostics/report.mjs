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
    if (report.status !== "passed-with-known-defects" || !report.streams?.length) throw new Error(`Incomplete diagnostic run: ${path}`);
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
    console.log(JSON.stringify({ metadata: report.metadata, backend: report.lastFrame.backend,
        idle: { ms: report.idle.elapsedMs, frames: report.idle.frames, cpuPercentOfOneCore: report.idle.cpuPercentOfOneCore,
            rssBefore: report.idle.before.rssBytes, rssAfter: report.idle.after.rssBytes },
        milestones: report.stages.map(stage => ({ name: stage.name, elapsedMs: stage.elapsedMs })),
        final: { native: report.lastFrame.elementCount, subjects: report.lastFrame.internalSubjectCount,
            fibers: report.bridge.bridge.fiberCount, registrations: report.bridge.registrations.tableCount },
    }, null, 2));
}
