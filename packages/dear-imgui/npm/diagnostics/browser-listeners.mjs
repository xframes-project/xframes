/** Injected before page scripts. Count actual persistent DOM registrations;
 * no timer/RAF interception and no per-frame instrumentation. */
export function installListenerAudit() {
    const prototype = EventTarget.prototype;
    const add = prototype.addEventListener, remove = prototype.removeEventListener;
    const records = [];
    let phase = "page", highWater = 0;
    const targetName = target => target === window ? "window" : target === document ? "document"
        : target instanceof HTMLCanvasElement ? "canvas" : null;
    const captureOf = options => typeof options === "boolean" ? options : !!options?.capture;
    prototype.addEventListener = function(type, listener, options) {
        const target = targetName(this), capture = captureOf(options);
        if (!target || !listener) return add.call(this, type, listener, options);
        const existing = records.find(record => record.target === this && record.type === type && record.listener === listener && record.capture === capture);
        if (existing) return add.call(this, type, existing.wrapped, options);
        if (records.length >= 256) throw new Error("DOM listener audit capacity exceeded");
        const record = { target: this, name: target, type, listener, wrapped: listener, capture, phase };
        const forget = () => { const index = records.indexOf(record); if (index >= 0) records.splice(index, 1); };
        if (typeof options === "object" && options?.once) record.wrapped = function(event) {
            forget();
            return typeof listener === "function" ? listener.call(this, event) : listener.handleEvent(event);
        };
        add.call(this, type, record.wrapped, options);
        if (!options?.signal?.aborted) {
            records.push(record);
            if (options?.signal) add.call(options.signal, "abort", forget, { once: true });
            highWater = Math.max(highWater, records.length);
        }
    };
    prototype.removeEventListener = function(type, listener, options) {
        const capture = captureOf(options);
        const index = records.findIndex(record => record.target === this && record.type === type && record.listener === listener && record.capture === capture);
        const record = index < 0 ? undefined : records.splice(index, 1)[0];
        return remove.call(this, type, record?.wrapped ?? listener, options);
    };
    globalThis.__xframesListenerAudit = {
        phase: value => { phase = value; },
        snapshot: () => ({ capacity: 256, highWater, native: records.filter(record => record.phase === "native").length,
            registrations: records.map(record => ({ target: record.name, type: record.type, capture: record.capture, phase: record.phase })) }),
    };
}
