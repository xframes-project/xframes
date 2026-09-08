import type { NativeBinding } from "./bridge";
import { check, waitFor } from "./assertions";
import { observeNativeFrame, readDiagnostics, waitForNativeIdle } from "./frames";

/** Runs on the same live module after acknowledged empty publication. Queries,
 * suspended publication and restoration must work without a rescue render loop. */
export async function verifyScheduling(binding: NativeBinding, unavailableScreenshot?: () => Promise<void>) {
    await waitForNativeIdle(binding, "empty runtime settles before scheduler checks");
    const beforeQueries = readDiagnostics(binding).scheduler;
    for (let i = 0; i < 100; ++i) {
        binding.getDiagnostics(); binding.getCommitState(); binding.isElementAlive(1900000001);
    }
    const afterQueries = readDiagnostics(binding).scheduler;
    check(afterQueries.generation === beforeQueries.generation, "Queries invalidated the runtime");
    const revision = JSON.parse(binding.getCommitState()).nativeRevision;
    binding.resizeWindow(0, 0);
    const suspended = await waitFor(() => readDiagnostics(binding), value => !value.scheduler.renderable,
        "zero-sized surface suspends drawing");
    try {
        const applied = JSON.parse(binding.applyCommit(JSON.stringify({ schemaVersion: 2, surfaceId: 0, baseRevision: revision,
            rootChildren: [], operations: [] })));
        check(applied.status === "applied", "Publication failed while surface was temporarily unavailable");
        const pending = readDiagnostics(binding).scheduler;
        check(pending.dirty && BigInt(pending.generation) > BigInt(pending.coveredGeneration), "Suspension lost pending invalidation");
        if (unavailableScreenshot) {
            let failure: unknown;
            try { await unavailableScreenshot(); } catch (error) { failure = error; }
            check(failure && String(failure).includes("unavailable"), "Screenshot on a zero-sized framebuffer did not fail explicitly");
        }
        // Observation interval, never used as evidence that a frame completed.
        await new Promise(resolve => setTimeout(resolve, 250));
        const paused = readDiagnostics(binding).scheduler;
        check(paused.constructed === suspended.scheduler.constructed && paused.submitted === suspended.scheduler.submitted,
            "Unavailable surface constructed or submitted frames");
        check(paused.coveredGeneration === suspended.scheduler.coveredGeneration, "Unavailable surface falsely advanced coverage");
    } finally {
        binding.resizeWindow(900, 700);
    }
    const resumed = await observeNativeFrame(binding, frame => frame.elementCount === 0, "restoration covers pending publication");
    await waitForNativeIdle(binding, "restored surface settles");
    check(BigInt(resumed.nativeRevision) === BigInt(revision) + 1n, "Window requests changed structural revision");
    return { status: "passed", queries: 300, suspendedPublication: "covered after restoration",
        unavailableScreenshot: unavailableScreenshot ? "explicit failure" : "not a native browser API",
        finalScheduler: readDiagnostics(binding).scheduler };
}
