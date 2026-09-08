import { createServer } from "node:http";
import { deflateSync } from "node:zlib";
import { randomUUID } from "node:crypto";
import { cpSync, mkdirSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";

// A deterministic PNG fixture, kept as source data rather than a generated binary.
const crc32 = bytes => {
    let value = 0xffffffff;
    for (const byte of bytes) {
        value ^= byte;
        for (let bit = 0; bit < 8; ++bit) value = (value >>> 1) ^ (value & 1 ? 0xedb88320 : 0);
    }
    return (value ^ 0xffffffff) >>> 0;
};
const chunk = (type, data) => {
    const body = Buffer.concat([Buffer.from(type), data]);
    const length = Buffer.alloc(4), checksum = Buffer.alloc(4);
    length.writeUInt32BE(data.length); checksum.writeUInt32BE(crc32(body));
    return Buffer.concat([length, body, checksum]);
};
const header = Buffer.from([0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0]);
const pixels = Buffer.from([0, 255, 80, 20, 255, 20, 180, 80, 255, 0, 20, 80, 255, 255, 255, 210, 20, 255]);
const png = Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    chunk("IHDR", header), chunk("IDAT", deflateSync(pixels)), chunk("IEND", Buffer.alloc(0))]);

export async function startResourceServer(root, output) {
    const assets = resolve(output, "resource-assets", randomUUID());
    mkdirSync(assets, { recursive: true });
    cpSync(resolve(root, "../assets/fonts"), resolve(assets, "fonts"), { recursive: true });
    writeFileSync(resolve(assets, "fixture.png"), png);
    writeFileSync(resolve(assets, "script.txt"), "\n");
    writeFileSync(resolve(assets, "invalid.png"), "invalid image fixture");
    const groups = new Map();
    const group = id => {
        if (!groups.has(id)) {
            if (groups.size >= 128) throw new Error("Resource fixture group limit exceeded");
            groups.set(id, { started: 0, finished: 0, cancelled: 0, released: false, pending: new Set() });
        }
        return groups.get(id);
    };
    const handler = (request, response) => {
        response.setHeader("Access-Control-Allow-Origin", "*");
        response.setHeader("Cross-Origin-Resource-Policy", "cross-origin");
        response.setHeader("Cache-Control", "no-store");
        try {
            const url = new URL(request.url, "http://127.0.0.1");
            const id = url.searchParams.get("group") ?? "default";
            const state = group(id);
            if (url.pathname === "/reset") {
                if (state.pending.size) { response.statusCode = 409; response.end("requests still pending"); return; }
                state.started = state.finished = state.cancelled = 0;
                state.released = false;
                response.end("reset");
                return;
            }
            if (url.pathname === "/state") {
                response.setHeader("Content-Type", "application/json");
                response.end(JSON.stringify({ started: state.started, finished: state.finished,
                    cancelled: state.cancelled, pending: state.pending.size, released: state.released }));
                return;
            }
            if (url.pathname === "/release") {
                state.released = true;
                for (const complete of [...state.pending]) complete();
                response.end("released");
                return;
            }
            if (url.pathname !== "/asset") { response.statusCode = 404; response.end(); return; }
            ++state.started;
            const complete = () => {
                if (!state.pending.delete(complete)) return;
                ++state.finished;
                const kind = url.searchParams.get("kind");
                response.statusCode = kind === "failure" ? 404 : 200;
                response.setHeader("Content-Type", kind === "script" ? "text/plain" : "image/png");
                response.end(kind === "script" ? "\n" : kind === "invalid" || kind === "failure" ? "invalid image fixture" : png);
            };
            state.pending.add(complete);
            response.on("close", () => { if (state.pending.delete(complete)) ++state.cancelled; });
            if (state.released || url.searchParams.get("hold") !== "1") complete();
        } catch (error) { response.statusCode = 500; response.end(String(error)); }
    };
    // Browsers cap concurrent HTTP/1 connections per origin. Six deliberately
    // held Canvas downloads must not prevent the controller from releasing them.
    const server = createServer(handler), controller = createServer(handler);
    const close = () => Promise.all([server, controller].map(instance => new Promise(resolveClose => {
        instance.closeAllConnections(); instance.close(resolveClose);
    })));
    try {
        await Promise.all([server, controller].map(instance => new Promise((resolveListen, reject) => {
            instance.once("error", reject); instance.listen(0, "127.0.0.1", resolveListen);
        })));
    } catch (error) { await close(); throw error; }
    return { baseUrl: `http://127.0.0.1:${server.address().port}`,
        controlUrl: `http://127.0.0.1:${controller.address().port}`, assets, close };
}
