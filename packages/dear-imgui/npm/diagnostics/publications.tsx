import React from "react";
import { check } from "./assertions";
import type { createBridge, NativeBinding } from "./bridge";
import type { NativeCommitState } from "@xframes/common";

const distribution = (values: number[]) => {
    const sorted = values.filter(Number.isFinite).sort((a, b) => a - b);
    const at = (p: number) => sorted.length ? sorted[Math.max(0, Math.ceil(sorted.length * p) - 1)] : null;
    return { samples: sorted.length, p50: at(.5), p95: at(.95), p99: at(.99), maximum: at(1) };
};

/** Measures actual ordinary completeRoot publications; React bailouts stay visible. */
export async function measureFabricPublications(bridge: ReturnType<typeof createBridge>, native: NativeBinding, repetitions: number) {
    const results: unknown[] = [];
    const makeTree = (generation: number, updated: boolean, reversed: boolean) => {
        const order = Array.from({ length: 12 }, (_, i) => i);
        if (reversed) order.reverse();
        return React.createElement("node", { root: true, id: "structural-root", style: { width: 700, height: 600 } },
            React.createElement("node", { id: "structural-group" }, order.map(index =>
                React.createElement("di-button", { key: `${generation}-${index}`, id: `structural-${index}`,
                    label: `${updated ? "Updated" : "Initial"} ${index}` }))));
    };
    for (let repetition = 0; repetition < repetitions; repetition++) {
        let requestedUpdates = 0, publications = 0, bailouts = 0, wireBytes = 0;
        const samples: Record<string, number[]> = Object.fromEntries([
            "requestedUpdate", "staging", "diff", "serialization", "boundary", "publicationTotal", "parse", "validationAndReachability",
            "application", "visibilityLockWait", "visibilityLockHeld",
        ].map(name => [name, []]));
        const phases: Record<string, { requested: number; publications: number }> = {};
        for (let cycle = 0; cycle < 40; cycle++) {
            const mounted = makeTree(cycle * 2, false, false);
            for (const [phase, element] of [
                ["mount", mounted], ["same-element-bailout", mounted],
                ["props", makeTree(cycle * 2, true, false)], ["reorder", makeTree(cycle * 2, true, true)],
                ["replacement", makeTree(cycle * 2 + 1, true, true)], ["unmount", null],
            ] as const) {
                const before = bridge.manager.getDiagnostics();
                const nativeBefore: NativeCommitState = JSON.parse(native.getCommitState());
                const start = performance.now();
                requestedUpdates++;
                await bridge.render(element);
                samples.requestedUpdate.push(performance.now() - start);
                const after = bridge.manager.getDiagnostics();
                const current: NativeCommitState = JSON.parse(native.getCommitState());
                const count = after.publications - before.publications;
                check(count === after.structuralCalls - before.structuralCalls && count === after.appliedPublications - before.appliedPublications,
                    `${phase}: exactly one structural call for every accepted completeRoot`);
                check(BigInt(current.nativeRevision) - BigInt(nativeBefore.nativeRevision) === BigInt(count), `${phase}: one native revision per completeRoot`);
                check(after.failedPublications === 0 && after.stagingNodeCount === 0 && after.retainedCandidateCount === 0,
                    `${phase}: publication failed or bridge retained staging work`);
                check(count <= 1, `${phase}: serial fixture unexpectedly produced multiple publications; last-publication timing would be incomplete`);
                publications += count; bailouts += Number(count === 0);
                const phaseCounts = phases[phase] ??= { requested: 0, publications: 0 };
                phaseCounts.requested++; phaseCounts.publications += count;
                samples.staging.push(after.stagingMs - before.stagingMs);
                if (!count) continue;
                const js = after.lastPublication!, cpp = current.lastTransaction!;
                check(js.status === "applied" && cpp.status === "applied", "Missing successful publication timing");
                wireBytes += js.wireBytes;
                samples.diff.push(js.diffMs); samples.serialization.push(js.serializationMs);
                samples.boundary.push(js.boundaryMs); samples.publicationTotal.push(js.totalMs);
                samples.parse.push(cpp.parseMs!); samples.validationAndReachability.push(cpp.validationMs!);
                samples.application.push(cpp.applicationMs!); samples.visibilityLockWait.push(cpp.visibilityLockWaitMs!);
                samples.visibilityLockHeld.push(cpp.visibilityLockHeldMs!);
                check(current.managedCount === (phase === "unmount" ? 0 : 14), `${phase}: complete native candidate count`);
            }
            check(bridge.manager.getDiagnostics().committedDescriptionCount === 0 && bridge.registrations.getDiagnostics().nativeCount === 0,
                "Structural workload cleanup retained committed ownership");
        }
        check(bailouts === 40 && publications === 200, "Expected 200 completeRoot publications and 40 same-element bailouts");
        results.push({ repetition, requestedUpdates, publications, bailouts, boundaryCalls: publications, wireBytes, phases,
            stagingHighWater: bridge.manager.getDiagnostics().stagingHighWater,
            ...Object.fromEntries(Object.entries(samples).map(([name, values]) => [`${name}Ms`, distribution(values)])) });
    }
    return { status: "passed", cyclesPerRepetition: 40, mountedNodeCount: 14, results,
        timingScope: "Requested update through Fabric callback; prospective staging summed separately. Native validation includes reachability. No frame/presentation latency claim." };
}
