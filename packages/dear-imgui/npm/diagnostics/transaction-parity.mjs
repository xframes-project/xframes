import assert from "node:assert/strict";
import { readFileSync, readdirSync, statSync } from "node:fs";
import { resolve, join, basename } from "node:path";

function readReport(input) {
    const path = resolve(input);
    if (!statSync(path).isDirectory()) {
        const json = JSON.parse(readFileSync(path, "utf8"));
        return json.report ?? json;
    }
    const reports = [];
    const visit = dir => {
        for (const entry of readdirSync(dir, { withFileTypes: true })) {
            const child = join(dir, entry.name);
            if (entry.isDirectory()) visit(child);
            else if (entry.name === "result.json" && ["node", "wasm"].includes(basename(dir))) {
                const report = readReport(child);
                if (report.transactions) reports.push(report);
            }
        }
    };
    visit(path);
    assert.equal(reports.length, 1, `Expected exactly one transaction report under ${path}`);
    return reports[0];
}

const args = process.argv.slice(2);
assert.equal(args.length, 2, "Pass the Node and Wasm result.json files (or their artifact directories)");
const [node, wasm] = args.map(readReport);
assert.equal(node.metadata.runtime, "Node/OpenGL");
assert.equal(wasm.metadata.runtime, "Wasm/WebGPU");
for (const report of [node, wasm]) {
    assert.equal(report.status, "passed");
    assert.equal(report.transactions.status, "passed");
    assert.ok(report.transactions.semanticResults.length >= 40, "Missing binding transaction fixtures");
}
assert.deepEqual(node.transactions.semanticResults, wasm.transactions.semanticResults, "Wire results/revisions/error classifications differ");
assert.deepEqual(node.transactions.mounted, wasm.transactions.mounted, "Populated native widget/Yoga state differs");
assert.deepEqual(node.transactions.finalState, wasm.transactions.finalState, "Native destruction results differ");
console.log(`Transaction binding parity passed: ${node.transactions.semanticResults.length} shared results, populated widgets, hierarchy/Yoga and empty native state`);
