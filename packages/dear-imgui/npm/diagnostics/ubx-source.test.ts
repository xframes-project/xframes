import assert from "node:assert/strict";
import { deliverScheduledUbx } from "./ubx-source";

async function test() {
    for (const hz of [20, 120]) {
        let now = 0;
        let job: { at: number; callback: () => void } | null = null;
        const received: number[] = [];
        const source = deliverScheduledUbx(hz * 60, hz, 0, sequence => received.push(sequence), {
            now: () => now,
            schedule: (callback, delay) => {
                assert.equal(job, null);
                job = { at: now + Math.max(16, delay), callback };
            },
        });
        while (job) {
            const current = job as { at: number; callback: () => void }; job = null;
            now = current.at; current.callback();
        }
        await source.completion;
        assert.deepEqual(received, Array.from({ length: hz * 60 }, (_, index) => index + 1));
        assert.ok(now >= 60000 && now <= 60016, "coarse timers must not halve source rate");
        assert.ok(source.diagnostics().maximumBatch <= 32);
        assert.ok(source.deadlineLatenessMs.every(value => value >= -1e-9));
    }
    let now = 0, job: (() => void) | null = null;
    const source = deliverScheduledUbx(120, 120, 0, () => {}, { now: () => now,
        schedule: callback => { assert.equal(job, null); job = callback; } });
    now = 1000;
    while (job) { const current = job as () => void; job = null; current(); }
    await source.completion;
    assert.equal(source.diagnostics().maximumBatch, 32);
    assert.equal(source.deliveredAt.length, 120);
    console.log("UBX source: exact 20/120-Hz sequences with coarse timers, no early packets and bounded delayed-source batches passed");
}
test().catch(error => { console.error(error); process.exitCode = 1; });
