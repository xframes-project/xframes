export type InvariantResult = { name: string; status: "pass" | "known-failure"; defect?: string; evidence?: unknown };

export function check(condition: unknown, message: string): asserts condition {
    if (!condition) throw new Error(message);
}

/** XFAIL is narrow and executing: a changed signature or an XPASS fails the run. */
export function knownFailure(name: string, invariant: boolean, defect: string,
    matchesKnownSignature: boolean, evidence: unknown): InvariantResult {
    check(!invariant, `${defect}: unexpected pass; review/remove the expected failure for ${name}`);
    check(matchesKnownSignature, `${defect}: failure signature changed for ${name}: ${JSON.stringify(evidence)}`);
    return { name, status: "known-failure", defect, evidence };
}

export async function waitFor<T>(read: () => T | Promise<T>, predicate: (value: T) => boolean,
    description: string, timeoutMs = 10_000, intervalMs = 5): Promise<T> {
    const start = performance.now();
    let last: T;
    while (performance.now() - start < timeoutMs) {
        last = await read();
        if (predicate(last)) return last;
        await new Promise(resolve => setTimeout(resolve, intervalMs));
    }
    throw new Error(`Timed out: ${description}; last observation: ${JSON.stringify(last!)}`);
}
