export type InvariantResult = { name: string; status: "pass"; defect?: string; evidence?: unknown };

export function check(condition: unknown, message: string): asserts condition {
    if (!condition) throw new Error(message);
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
