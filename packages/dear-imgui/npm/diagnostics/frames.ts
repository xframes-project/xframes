import type { NativeDiagnostics, NativeFrameSnapshot, NativeCounter } from "@xframes/common";
import type { NativeBinding } from "./bridge";
import { check, waitFor } from "./assertions";

export type NativeFrame = NativeDiagnostics & NativeFrameSnapshot;
export const readDiagnostics = (binding: NativeBinding): NativeDiagnostics => JSON.parse(binding.getDiagnostics());
export function counterDelta(after: NativeCounter, before: NativeCounter): number {
    const delta = Number(BigInt(after) - BigInt(before));
    check(Number.isSafeInteger(delta) && delta >= 0, "Measurement counter delta exceeds safe integer range");
    return delta;
}
/** Capture an explicit target after the operation. Already-covered work, including
 * a rejected publication, can complete immediately without manufacturing a frame. */
export async function observeNativeFrame(binding: NativeBinding, predicate: (frame: NativeFrame) => boolean, label: string) {
    const generation = readDiagnostics(binding).scheduler.generation;
    const revision = JSON.parse(binding.getCommitState()).nativeRevision as NativeCounter;
    const result = await waitFor(() => readDiagnostics(binding), next => {
        check(next.scheduler.status === "running", `${label}: terminal renderer ${next.scheduler.status}`);
        return next.enabled && typeof next.frameId === "string"
            && BigInt(next.coveredGeneration!) >= BigInt(generation) && BigInt(next.nativeRevision!) >= BigInt(revision)
            && predicate(next as NativeFrame);
    }, label);
    return result as NativeFrame;
}

export function waitForNativeIdle(binding: NativeBinding, label: string) {
    return waitFor(() => readDiagnostics(binding), next => {
        const state = next.scheduler;
        check(state.status === "running", `${label}: terminal renderer ${state.status}`);
        return state.renderable && !state.dirty && state.activeOwners === 0 && state.deadlines === 0
            && state.deferredUntilMs === null;
    }, label);
}
