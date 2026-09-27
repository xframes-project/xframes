import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { inflateSync } from "node:zlib";
import React from "react";
import { ImGuiCol, ImGuiStyleVar } from "@xframes/common";
import { createBridge } from "./bridge";
import { observeNativeFrame } from "./frames";
import { nativeInput } from "./node-input";
import { waitFor } from "./assertions";

// Load the actual CMake Release target, never an installed package or a mock.
const addon = resolve("node/build/Release/xframes.node");
const native = require(addon);
const output = resolve(process.env.XFRAMES_DIAGNOSTICS_DIR ?? "build/diagnostics/styles-node");
mkdirSync(output, { recursive: true });
const mode = process.env.NODE_ENV ?? "development";
const families = ["style", "hoverStyle", "activeStyle", "disabledStyle"] as const;
const teal = "#149c91";
const publications: any[] = [];
const stages: unknown[] = [];
let expectedClose = false, closed = false;

// Decode only the 8-bit, non-interlaced RGB/RGBA PNGs produced by the native
// screenshot writer. Pixel assertions must inspect rendered output, not props.
function countTealPixels(path: string) {
    const png = readFileSync(path);
    assert.equal(png.subarray(0, 8).toString("hex"), "89504e470d0a1a0a");
    assert.equal(png.subarray(12, 16).toString(), "IHDR");
    assert.equal(png[24], 8);
    assert.ok(png[25] === 2 || png[25] === 6);
    assert.deepEqual([...png.subarray(26, 29)], [0, 0, 0]);
    const width = png.readUInt32BE(16), height = png.readUInt32BE(20), channels = png[25] === 6 ? 4 : 3;
    const chunks: Buffer[] = [];
    for (let offset = 8; offset < png.length;) {
        const length = png.readUInt32BE(offset);
        if (png.subarray(offset + 4, offset + 8).toString() === "IDAT") chunks.push(png.subarray(offset + 8, offset + 8 + length));
        offset += length + 12;
    }
    const raw = inflateSync(Buffer.concat(chunks)), stride = width * channels;
    assert.equal(raw.length, (stride + 1) * height);
    let previous = Buffer.alloc(stride), count = 0;
    for (let y = 0; y < height; ++y) {
        const row = Buffer.alloc(stride), filter = raw[y * (stride + 1)];
        assert.ok(filter <= 4);
        for (let x = 0; x < stride; ++x) {
            const left = x < channels ? 0 : row[x - channels], up = previous[x];
            const corner = x < channels ? 0 : previous[x - channels];
            const p = left + up - corner;
            const a = Math.abs(p - left), b = Math.abs(p - up), c = Math.abs(p - corner);
            const predictor = filter === 0 ? 0 : filter === 1 ? left : filter === 2 ? up
                : filter === 3 ? Math.floor((left + up) / 2) : a <= b && a <= c ? left : b <= c ? up : corner;
            row[x] = raw[y * (stride + 1) + 1 + x] + predictor;
        }
        for (let x = 0; x < stride; x += channels)
            if (row[x] === 0x14 && row[x + 1] === 0x9c && row[x + 2] === 0x91) ++count;
        previous = row;
    }
    return count;
}

function family(fontSize: number) {
    return { width: 320, font: { name: "roboto-regular", size: fontSize },
        colors: { [ImGuiCol.Button]: teal, [ImGuiCol.ButtonHovered]: teal, [ImGuiCol.ButtonActive]: teal },
        vars: { [ImGuiStyleVar.FramePadding]: [12, 10], [ImGuiStyleVar.FrameRounding]: 4 } };
}
const headingStyles: Record<string, any> = Object.fromEntries(families.map(name => [name, family(28)]));
const buttonStyles: Record<string, any> = Object.fromEntries(families.map(name => [name, family(18)]));
let text = "Encrypt a file", label = "Encrypt";
function Fixture() {
    // Deliberately fresh objects on every render: Fabric still omits styles
    // whose values are unchanged, just as it does in an ordinary application.
    return React.createElement("node", { root: true, style: { width: "100%", height: "100%", padding: { all: 28 } } },
        React.createElement("unformatted-text", { id: "heading", text, ...JSON.parse(JSON.stringify(headingStyles)) }),
        React.createElement("di-button", { id: "action", label, ...JSON.parse(JSON.stringify(buttonStyles)) }));
}

async function main() {
    const callbacks = ["onTextChange", "onComboChange", "onNumericValueChange", "onBooleanValueChange", "onMultiValueChange",
        "onClick", "onTableSort", "onTableFilter", "onTableRowClick", "onTableItemAction", "onPrefetchProgress", "onScriptError"];
    await new Promise<void>(ready => native.init({
        assetsBasePath: resolve("../assets"), theme: "{}",
        fontDefs: JSON.stringify({ defs: [14, 18, 28].map(size => ({ name: "roboto-regular", size })) }),
        ...Object.fromEntries(callbacks.map(name => [name, () => {}])), onInit: ready,
        onBeforeExit: () => { closed = true; if (!expectedClose) process.exit(1); },
    }));
    native.setDiagnosticsEnabled(true);
    const bridge = createBridge({ ...native, applyCommit(wire: string) {
        publications.push(JSON.parse(wire));
        return native.applyCommit(wire);
    } });
    try {
        await bridge.render(React.createElement(Fixture));
        // Keep interaction state deterministic while checking base styling.
        await nativeInput({ action: "move", x: 800, y: 600 });
        const id = (publicId: string) => bridge.registrations.getDiagnostics().mappings.find(item => item.publicId === publicId)!.nativeId;
        const headingId = id("heading"), buttonId = id("action");
        const inspect = async (stage: string, styled = true) => {
            const frame = await observeNativeFrame(native, () => true, stage);
            const heading = frame.elements.find(item => item.id === headingId)!;
            const button = frame.elements.find(item => item.id === buttonId)!;
            const screenshot = resolve(output, `${mode}-${stage}.png`);
            await new Promise<void>((done, reject) => native.captureScreenshot(screenshot,
                (error: string | null) => error ? reject(new Error(error)) : done()));
            const tealPixels = countTealPixels(screenshot);
            stages.push({ stage, headingBounds: heading.bounds, buttonBounds: button.bounds, tealPixels,
                nativeRevision: frame.nativeRevision, screenshot });
            assert.equal(heading.bounds[3], styled ? 28 : 14, `${stage}: heading font height`);
            assert.equal(button.bounds[3], styled ? 38 : 20, `${stage}: button font and FramePadding`);
            assert.ok(styled ? tealPixels > 1000 : tealPixels === 0, `${stage}: rendered button color (${tealPixels} teal pixels)`);
        };
        const renderPatch = async (headingKeys: string[], buttonKeys: string[]) => {
            const start = publications.length;
            await bridge.render(React.createElement(Fixture));
            const patches = publications.slice(start).flatMap(batch => batch.operations).filter(op => op.op === "patch");
            assert.deepEqual(patches.map(op => op.id).sort((a, b) => a - b), [headingId, buttonId].sort((a, b) => a - b));
            assert.deepEqual(Object.keys(patches.find(op => op.id === headingId).props).sort(), [...headingKeys].sort());
            assert.deepEqual(Object.keys(patches.find(op => op.id === buttonId).props).sort(), [...buttonKeys].sort());
        };
        await inspect("initial");
        text = "Decrypt a file"; label = "Working...";
        await renderPatch(["text"], ["label"]);
        await inspect("text-and-label-only");
        for (const name of families) {
            headingStyles[name] = { ...family(28), vars: { ...family(28).vars, [ImGuiStyleVar.FrameRounding]: 8 } };
            buttonStyles[name] = { ...family(18), vars: { ...family(18).vars, [ImGuiStyleVar.FrameRounding]: 8 } };
            await renderPatch([name], [name]);
            await inspect(`replace-${name}`);
        }
        for (const name of families.slice(1)) {
            headingStyles[name] = null; buttonStyles[name] = null;
            await renderPatch([name], [name]);
            await inspect(`clear-${name}`);
        }
        headingStyles.style = null; buttonStyles.style = null;
        await renderPatch(["style"], ["style"]);
        await inspect("clear-base", false);
        headingStyles.style = family(28); buttonStyles.style = family(18);
        await renderPatch(["style"], ["style"]);
        await inspect("restore-base");
        headingStyles.style = {}; buttonStyles.style = {};
        await renderPatch(["style"], ["style"]);
        await inspect("empty-base", false);
        assert.deepEqual(bridge.rendererErrors, []);
    } finally {
        await bridge.dispose();
        expectedClose = true;
        await nativeInput({ action: "close" });
        await waitFor(() => closed, Boolean, "style test window close");
    }
}

const watchdog = setTimeout(() => { console.error("Native style test timed out"); process.exit(1); }, 60_000);
main().then(() => finish(), error => finish(error));
function finish(error?: unknown) {
    clearTimeout(watchdog);
    writeFileSync(resolve(output, `${mode}-styles.json`), JSON.stringify({ status: error ? "fail" : "pass", mode, addon,
        addonSha256: createHash("sha256").update(readFileSync(addon)).digest("hex"), stages, publications,
        ...(error ? { error: String(error) } : {}) }, null, 2));
    if (error) console.error(error);
    else console.log(`Native Fabric style patching passed (${mode}, ${stages.length} rendered stages)`);
    process.exit(error ? 1 : 0);
}
