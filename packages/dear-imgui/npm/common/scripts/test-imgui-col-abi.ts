import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { ImGuiCol } from "../src/lib/types.js";

// Compare the public numeric wire API to the native dependency it configures.
// This catches inserted/reordered colors as well as renamed compatibility aliases.
const header = readFileSync(resolve(__dirname, "../../../cpp/deps/imgui/imgui.h"), "utf8");
const body = header.split("enum ImGuiCol_\n")[1]?.split("};")[0]
    ?? header.split("enum ImGuiCol_\r\n")[1]?.split("};")[0];
assert.ok(body, "Missing native ImGuiCol definition");
const native: Record<string, number> = {};
let next = 0;
for (const match of body.matchAll(/^\s*ImGuiCol_(\w+)(?:\s*=\s*ImGuiCol_(\w+))?\s*,/gm)) {
    const [, name, alias] = match;
    native[name] = alias ? native[alias] : next++;
    assert.equal(ImGuiCol[name as keyof typeof ImGuiCol], native[name], `Native ImGuiCol_${name} ABI`);
}
assert.equal(native.COUNT, ImGuiCol.COUNT);
assert.ok(Object.keys(native).length > 60);
console.log(`ImGuiCol ABI: ${Object.keys(native).length} native names and aliases match`);
