export interface SourceClock {
    now(): number;
    schedule(callback: () => void, delayMs: number): unknown;
}

/** Source deadlines do not depend on UI frames. A coarse/delayed host timer may
 * deliver several due wire packets, as a serial callback can. Limit each turn
 * to 32 pairs so input debt cannot monopolize the JS loop. UI pacing is separate
 * and never catches up missed publication ticks.
 */
export function deliverScheduledUbx(count: number, hz: number, start: number,
    send: (sequence: number) => void, clock: SourceClock = {
        now: () => performance.now(), schedule: (callback, ms) => setTimeout(callback, ms),
    }) {
    const deliveredAt: number[] = [];
    const deadlineLatenessMs: number[] = [];
    let timerCallbacks = 0, maximumBatch = 0;
    const completion = new Promise<void>((resolve, reject) => {
        let next = 1;
        const tick = () => {
            timerCallbacks++;
            try {
                let batch = 0;
                const due = Math.min(count, Math.floor((clock.now() - start) * hz / 1000 + 1e-9));
                while (next <= due && batch < 32) {
                    const receivedAt = clock.now();
                    send(next);
                    deliveredAt.push(clock.now() - start);
                    deadlineLatenessMs.push(receivedAt - (start + next * 1000 / hz));
                    next++; batch++;
                }
                maximumBatch = Math.max(maximumBatch, batch);
                if (next > count) resolve();
                else clock.schedule(tick, Math.max(0, start + next * 1000 / hz - clock.now()));
            } catch (error) { reject(error); }
        };
        clock.schedule(tick, Math.max(0, start + 1000 / hz - clock.now()));
    });
    return { completion, deliveredAt, deadlineLatenessMs,
        diagnostics: () => ({ timerCallbacks, maximumBatch, maximumAllowedBatch: 32 }) };
}
