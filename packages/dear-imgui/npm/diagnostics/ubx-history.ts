import assert from "node:assert/strict";

/** Bounded record of successful ordinary widget calls. Native submitted summaries
 * are checked separately: these arrays prove the ordered data sent across the
 * boundary, rather than claiming full native buffers are directly readable.
 */
export class UbxHistoryLedger {
    readonly lines = new Map<number, { x: number; y: number }[]>();
    readonly consoles = new Map<number, string>();
    readonly trails = new Map<number, { lat: number; lon: number }[]>();
    readonly scatters = new Map<number, { x: number; y: number }[]>();
    readonly tables = new Map<number, any[]>();
    consoleAppends = 0;
    consoleAppendedCodeUnits = 0;

    operation(id: number, type: string, op: any) {
        if (type === "plot-line") {
            if (op.op === "resetData") this.lines.set(id, []);
            if (op.op === "appendData") {
                const points = this.lines.get(id) ?? [];
                points.push({ x: op.x, y: op.y });
                if (points.length > 3000) points.shift();
                this.lines.set(id, points);
            }
        }
        if (type === "plot-scatter") {
            if (op.op === "resetData") this.scatters.set(id, []);
            if (op.op === "setData") this.scatters.set(id, op.data);
        }
        if (type === "di-table" && op.op === "setData") this.tables.set(id, op.data);
        if (type === "map-view") {
            if (op.op === "setPolylines") this.trails.set(id, (op.polylines[0]?.points ?? []).slice());
            if (op.op === "appendPolylinePoint") {
                const points = this.trails.get(id) ?? [];
                points.push({ lat: op.lat, lon: op.lon });
                if (points.length > 1000) points.shift();
                this.trails.set(id, points);
            }
        }
    }
    appendConsole(id: number, text: string) {
        const retained = (this.consoles.get(id) ?? "") + text;
        assert.ok(retained.length <= 65536, "Console native calls exceed code-unit retention bound");
        this.consoles.set(id, retained);
        this.consoleAppends++; this.consoleAppendedCodeUnits += text.length;
    }
    destroy(id: number) {
        this.lines.delete(id); this.consoles.delete(id); this.trails.delete(id);
        this.scatters.delete(id); this.tables.delete(id);
    }
    snapshot() {
        return { lines: [...this.lines], consoles: [...this.consoles], trails: [...this.trails],
            scatters: [...this.scatters], tables: [...this.tables], consoleAppends: this.consoleAppends,
            consoleAppendedCodeUnits: this.consoleAppendedCodeUnits };
    }
}

/** Independent formatter for the two checksummed fixture packets. This oracle
 * sees generated input, never the application's retained text or formatter.
 */
export class UbxConsoleOracle {
    text = "";
    offset = 0;
    evictedCodeUnits = 0;
    packet(packet: Buffer, name: string) {
        let rendered = `\n──── ${name} [01:${packet[3].toString(16).padStart(2, "0")}] ${packet.length - 8} bytes ────\n`;
        for (let start = 0; start < packet.length; start += 16) {
            const bytes = [...packet.subarray(start, start + 16)];
            const cells = Array.from({ length: 16 }, (_, index) => index < bytes.length
                ? bytes[index].toString(16).padStart(2, "0") + " " : "   ");
            const ascii = Array.from({ length: 16 }, (_, index) => index >= bytes.length ? " "
                : bytes[index] >= 32 && bytes[index] <= 126 ? String.fromCharCode(bytes[index]) : ".").join("");
            rendered += (this.offset + start).toString(16).padStart(8, "0") + "  "
                + cells.slice(0, 8).join("") + " " + cells.slice(8).join("") + ` |${ascii}|\n`;
        }
        this.offset += packet.length;
        this.text += rendered;
        if (this.text.length > 65536) {
            this.evictedCodeUnits += this.text.length - 32768;
            this.text = this.text.slice(-32768);
        }
    }
    reset() { this.text = ""; this.offset = 0; this.evictedCodeUnits = 0; }
}

export function expectedPosition(sequence: number) {
    return { lat: (515000000 + sequence % 100 * 10) * 1e-7,
        lon: (-1200000 + sequence * 10) * 1e-7, altitude: (50000 + sequence) / 1000,
        speed: (1000 + sequence) * 0.0036, hAcc: 30 };
}

export function verifyUbxHistories(ledger: UbxHistoryLedger, nodes: Map<number, any>, lastSequence: number,
    consoleOracle: UbxConsoleOracle, positionFirst = 0, connectionFirst = 0) {
    const id = (type: string, predicate: (node: any) => boolean = () => true) => {
        const entry = [...nodes].find(([, node]) => node.type === type && predicate(node));
        assert.ok(entry, `Missing ${type}`); return entry[0];
    };
    const positions = Array.from({ length: Math.min(3000, lastSequence - positionFirst + 1) },
        (_, index) => Math.max(positionFirst, lastSequence - 2999) + index);
    const trail = Array.from({ length: Math.min(1000, lastSequence - connectionFirst + 1) },
        (_, index) => Math.max(connectionFirst, lastSequence - 999) + index);
    const actualTrail = ledger.trails.get(id("map-view")) ?? [];
    assert.deepEqual(actualTrail.map(point => Math.round((point.lon + .12) * 1e6)), trail,
        "Exact map trail sequence tail differs from source");
    actualTrail.forEach((point, index) => {
        const expected = expectedPosition(trail[index]);
        assert.ok(Math.abs(point.lat - expected.lat) < 1e-10 && Math.abs(point.lon - expected.lon) < 1e-10);
    });
    const lineChecks = [["Altitude (m)", "altitude"], ["Speed (km/h)", "speed"], ["H Accuracy (m)", "hAcc"]] as const;
    for (const [axis, field] of lineChecks) {
        const points = ledger.lines.get(id("plot-line", node => node.yAxisLabel === axis)) ?? [];
        assert.deepEqual(points.map(point => point.y), positions.map(sequence => expectedPosition(sequence)[field]), `Exact ${axis} tail`);
        assert.ok(points.every((point, index) => index === 0 || point.x >= points[index - 1].x), "Position times went backwards");
    }
    const sourcePositions = positions.map(expectedPosition);
    const meanLat = sourcePositions.reduce((sum, p) => sum + p.lat, 0) / positions.length;
    const meanLon = sourcePositions.reduce((sum, p) => sum + p.lon, 0) / positions.length;
    const scatter = ledger.scatters.get(id("plot-scatter")) ?? [];
    assert.equal(scatter.length, positions.length);
    scatter.forEach((point, index) => {
        const sample = sourcePositions[index];
        assert.ok(Math.abs(point.x - (sample.lon - meanLon) * Math.cos(meanLat * Math.PI / 180) * 111320) < 1e-6);
        assert.ok(Math.abs(point.y - (sample.lat - meanLat) * 111320) < 1e-6);
    });
    const rows = ledger.tables.get(id("di-table")) ?? [];
    const received = (lastSequence - connectionFirst + 1) * 2;
    assert.deepEqual(rows.map(row => row.sequence), Array.from({ length: Math.min(500, received) },
        (_, index) => Math.max(1, received - 499) + index), "Exact message arrival ordinals");
    const text = ledger.consoles.get(id("clipped-multi-line-text-renderer"));
    assert.equal(text, consoleOracle.text, "Exact Console retained text and final byte offset");
    return { lastSequence, positionFirst, connectionFirst, retained: { positions: positions.length, trail: trail.length,
        messages: rows.length, consoleCodeUnits: text!.length, consoleBytes: Buffer.byteLength(text!) },
        evicted: { positions: Math.max(0, lastSequence - positionFirst + 1 - 3000),
            trail: Math.max(0, lastSequence - connectionFirst + 1 - 1000), messages: Math.max(0, received - 500),
            consoleCodeUnits: consoleOracle.evictedCodeUnits }, consoleSourceBytes: consoleOracle.offset };
}
