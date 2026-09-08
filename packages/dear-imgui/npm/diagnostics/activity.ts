import type { NativeBinding } from "./bridge";
import { check, waitFor } from "./assertions";
import { counterDelta, observeNativeFrame, readDiagnostics, waitForNativeIdle } from "./frames";

/** Separate from the historical streaming workload. Identical continuously
 * executing Canvas content measures submitted cadence and diagnostic cost. */
export async function measureActivity(binding: NativeBinding, durationMs: number, resources: () => any) {
    const baseline = await waitForNativeIdle(binding, "activity measurement begins inactive");
    const root = 1905000000, canvas = root + 1;
    const apply = (operations: unknown[], rootChildren: number[]) => {
        const result = JSON.parse(binding.applyCommit(JSON.stringify({ schemaVersion: 2, surfaceId: 0,
            baseRevision: JSON.parse(binding.getCommitState()).nativeRevision, rootChildren, operations })));
        check(result.status === "applied", `Activity publication failed: ${JSON.stringify(result)}`);
    };
    const op = (value: object) => binding.elementInternalOp(canvas, JSON.stringify(value));
    const samples = [];
    try {
        apply([{ op: "create", id: root, elementType: "node", props: { root: true, style: { width: 900, height: 700 } } },
            { op: "create", id: canvas, elementType: "di-js-canvas", props: { style: { width: 400, height: 300 } } },
            { op: "setChildren", parentId: root, childrenIds: [canvas] },
            { op: "setChildren", parentId: canvas, childrenIds: [] }], [root]);
        // A moving rectangle exercises normal per-frame script execution/drawing.
        op({ op: "setScript", script: "globalThis.fixtureTick = (globalThis.fixtureTick || 0) + 1; ctx.fillStyle = '#40a0e0'; ctx.fillRect(fixtureTick % 350, 40, 40, 40);" });
        await observeNativeFrame(binding, frame => frame.scheduler.reasons.canvas.activeOwners === 1,
            "normal Canvas continuous execution starts");
        for (const enabled of [false, true]) {
            binding.setDiagnosticsEnabled(enabled);
            const warmup = readDiagnostics(binding).scheduler.submitted;
            await waitFor(() => readDiagnostics(binding), value => counterDelta(value.scheduler.submitted, warmup) >= 3,
                "active Canvas warmup");
            const before = readDiagnostics(binding), resourceBefore = resources(), start = performance.now();
            await new Promise(resolve => setTimeout(resolve, durationMs));
            const elapsedMs = performance.now() - start, after = readDiagnostics(binding), resourceAfter = resources();
            const submitted = counterDelta(after.scheduler.submitted, before.scheduler.submitted);
            const cpuBefore = resourceBefore.cpuMicroseconds, cpuAfter = resourceAfter.cpuMicroseconds;
            check(submitted > 0 && after.scheduler.reasons.canvas.activeOwners === 1, "Continuous Canvas stopped advancing");
            check(after.scheduler.completedFrame?.nativeRevision === before.scheduler.completedFrame?.nativeRevision,
                "Canvas execution unexpectedly created structural publications");
            samples.push({ diagnosticsEnabled: enabled, elapsedMs, submitted,
                submittedHz: submitted * 1000 / elapsedMs,
                constructed: counterDelta(after.scheduler.constructed, before.scheduler.constructed),
                opportunities: counterDelta(after.scheduler.opportunities, before.scheduler.opportunities),
                cpuPercentOfOneCore: cpuBefore && cpuAfter
                    ? (cpuAfter.user - cpuBefore.user + cpuAfter.system - cpuBefore.system) / (elapsedMs * 10) : null,
                before: resourceBefore, after: resourceAfter, platform: after.platform });
        }
        op({ op: "setContinuous", continuous: false });
        await waitForNativeIdle(binding, "static Canvas returns to inactivity");
        return { status: "passed", samples, workload: "One visible QuickJS Canvas drawing a moving rectangle; no per-frame host JS polling",
            limitations: "Submission cadence, not presentation; off/on order and host variation limit cost attribution" };
    } finally {
        binding.setDiagnosticsEnabled(true);
        apply([], []);
        await observeNativeFrame(binding, frame => frame.elementCount === 0, "activity cleanup covered");
        const empty = await waitForNativeIdle(binding, "activity cleanup settles");
        check(empty.scheduler.ownerCount === baseline.scheduler.ownerCount, "Activity measurement retained an owner");
    }
}
