import type { NativeBinding } from "./bridge";
import type { FixtureInput } from "./input";
import { check, waitFor } from "./assertions";
import { observeNativeFrame, readDiagnostics, waitForNativeIdle } from "./frames";

/** Real platform minimize/restore, independent of the explicit zero-size API. */
export async function verifyVisibility(binding: NativeBinding, input: (request: FixtureInput) => Promise<void>) {
    const initial = await waitForNativeIdle(binding, "visibility fixture starts from inactivity");
    const revision = JSON.parse(binding.getCommitState()).nativeRevision;
    await input({ action: "minimize" });
    const hidden = await waitFor(() => readDiagnostics(binding), next => !next.scheduler.renderable,
        "platform visibility event suspends drawing");
    try {
        check(hidden.platform.animationCallbacks === 0 && hidden.platform.deadlineTimers === 0,
            "Hidden renderer retained a browser animation callback or timer");
        const applied = JSON.parse(binding.applyCommit(JSON.stringify({ schemaVersion: 2, surfaceId: 0,
            baseRevision: revision, rootChildren: [], operations: [] })));
        check(applied.status === "applied", "Hidden publication failed");
        await new Promise(resolve => setTimeout(resolve, 250));
        const pending = readDiagnostics(binding);
        check(pending.scheduler.dirty && pending.scheduler.generation !== pending.scheduler.coveredGeneration,
            "Hidden publication lost its pending generation");
        check(pending.scheduler.constructed === hidden.scheduler.constructed && pending.scheduler.submitted === hidden.scheduler.submitted,
            "Hidden window constructed or submitted native frames");
    } finally { await input({ action: "restore" }); }
    const restored = await observeNativeFrame(binding, frame => frame.elementCount === 0, "restoration covers hidden publication");
    const idle = await waitForNativeIdle(binding, "restored window settles");
    check(BigInt(restored.nativeRevision) === BigInt(revision) + 1n, "Visibility changed structural ordering");
    check(idle.platform.animationCallbacks === 0 && idle.platform.deadlineTimers === 0,
        "Restored inactive renderer retained browser callbacks/timers");
    if (typeof window === "undefined") {
        await input({ action: "refresh" });
        await observeNativeFrame(binding, frame => BigInt(frame.scheduler.reasons.window.invalidations)
            > BigInt(idle.scheduler.reasons.window.invalidations), "native exposure callback wakes inactive renderer");
        await waitForNativeIdle(binding, "refreshed window settles");
    }
    return { status: "passed", hiddenRevision: revision, restoredRevision: restored.nativeRevision,
        source: typeof window === "undefined" ? "native window minimize/restore/exposure" : "Chromium window minimize/restore",
        platform: idle.platform, initialPlatform: initial.platform };
}
