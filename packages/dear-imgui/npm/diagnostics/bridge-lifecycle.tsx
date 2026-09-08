import assert from "node:assert/strict";
import { mkdirSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import React, { useEffect, useState } from "react";
import { createBridge, createFakeBinding } from "./bridge";
import { components } from "@xframes/common";
import { check, knownFailure, waitFor, type InvariantResult } from "./assertions";

const output = resolve(process.env.XFRAMES_DIAGNOSTICS_DIR ?? "./build/diagnostics");
const { Table, PlotBar } = components;
mkdirSync(output, { recursive: true });
const results: any[] = [];
const mode = process.env.NODE_ENV ?? "development";
const save = () => writeFileSync(resolve(output, `bridge-${mode}.json`), JSON.stringify({ mode, results }, null, 2));
const leaf = (id: string, label = id, onClick?: () => void) => React.createElement("di-button", { key: id, id, label, onClick });
const tree = (ids: string[]) => React.createElement("node", { root: true, id: "root" }, ids.map(id => leaf(id)));

async function scenario(name: string, run: (h: ReturnType<typeof createBridge>, f: ReturnType<typeof createFakeBinding>, checks: InvariantResult[]) => Promise<void>) {
    const fake = createFakeBinding();
    const bridge = createBridge(fake.binding);
    const checks: InvariantResult[] = [];
    try {
        await run(bridge, fake, checks);
        results.push({ name, status: "pass", checks, diagnostics: bridge.snapshot() });
    } catch (error) {
        results.push({ name, status: "fail", error: String(error), checks, diagnostics: bridge.snapshot() });
        throw error;
    } finally {
        bridge.dispose();
        assert.equal(bridge.manager.getDiagnostics().subscriptionClosed, true);
        save();
    }
}

async function main() {
  await scenario("mount, insert, reorder, replace, deep delete, unmount", async (bridge, fake, checks) => {
    await bridge.render(tree(["a", "b"]));
    assert.equal(fake.nodes.size, 4);
    const rootId = bridge.registrations.getDiagnostics().mappings.find(item => item.publicId === "root")!.nativeId;
    const before = [...fake.children.get(rootId)!];
    await bridge.render(tree(["b", "a"]));
    assert.deepEqual(fake.children.get(rootId), before.reverse());
    await bridge.render(tree(["b", "c", "a"]));
    assert.equal(fake.children.get(rootId)!.length, 3);
    await bridge.render(tree(["replacement"]));
    assert.equal(fake.nodes.size, 3);
    await bridge.render(React.createElement("node", { root: true, id: "root" },
        React.createElement("node", null, React.createElement("node", null, leaf("deep")))));
    assert.equal(fake.nodes.size, 5);
    await bridge.render(null);
    assert.equal(fake.nodes.size, 1);
    const state = bridge.snapshot();
    checks.push(knownFailure("unmount releases Fiber mappings", state.bridge.fiberCount === 0,
        "XF-LIFE-001", state.bridge.fiberCount === state.operations.counts.setElement,
        { retained: state.bridge.fiberCount, created: state.operations.counts.setElement }));
    const trace = state.operations.trace;
    const firstCommit = trace.findIndex(call => call.method === "completeRoot:enter");
    const earlyCreates = trace.slice(0, firstCommit).filter(call => call.method === "setElement").length;
    checks.push(knownFailure("prospective work stays out of native state", earlyCreates === 0,
        "XF-LIFE-004", earlyCreates === 3, { earlyCreates }));
  });

  await scenario("React state, prop diff, and rapid updates", async (bridge, fake) => {
    let update: React.Dispatch<React.SetStateAction<number>>;
    function Counter() {
        const [value, setValue] = useState(0);
        update = setValue;
        return React.createElement("node", { root: true }, leaf("counter", `value-${value}`));
    }
    await bridge.render(React.createElement(Counter));
    for (let value = 1; value <= 25; value++) update!(value);
    await waitFor(() => [...fake.nodes.values()].find(node => node.type === "di-button")?.label,
        label => label === "value-25", "batched state update reaches native props");
    check((bridge.observer.snapshot().counts.patchElement ?? 0) > 0, "No native prop patch observed");
    await bridge.render(null);
  });

  await scenario("removing a public ID preserves native identity", async (bridge, fake, checks) => {
    await bridge.render(React.createElement("node", { root: true, id: "public-root" }, leaf("a")));
    await bridge.render(React.createElement("node", { root: true }, leaf("a")));
    const rootChildren = fake.children.get(0)!;
    checks.push(knownFailure("public ID changes do not overwrite the native ID", rootChildren.every(id => typeof id === "number"),
        "XF-LIFE-007", rootChildren.length === 1 && rootChildren[0] === null, { rootChildren }));
  });

  await scenario("real PlotBar/Table registration and imperative handles", async (bridge, fake, checks) => {
    const plot = React.createRef<React.ComponentRef<typeof PlotBar>>();
    const table = React.createRef<React.ComponentRef<typeof Table>>();
    await bridge.render(React.createElement("node", { root: true },
        React.createElement(PlotBar, { ref: plot, series: [{ label: "A" }, { label: "B" }] }),
        React.createElement(Table, { ref: table, columns: [{ fieldId: "value", heading: "Value", type: "number" }] })));
    await waitFor(() => bridge.registrations.getDiagnostics().tableCount, count => count === 2, "passive registrations");
    plot.current!.setSeriesData([{ data: [{ x: 1, y: 3 }] }, { data: [{ x: 1, y: 4 }] }]);
    table.current!.setTableData([{ value: 42 }]);
    assert.equal(fake.internalOps.length, 2);
    assert.equal(fake.internalOps.every(op => op.live), true);
    const retainedHandle = plot.current!;
    const removing = bridge.render(null);
    retainedHandle.appendData(2, 5);
    const duringRemoval = fake.internalOps.at(-1)!;
    assert.equal(duringRemoval.op.op, "appendData");
    await removing;
    retainedHandle.appendData(3, 6);
    const last = fake.internalOps.at(-1)!;
    checks.push(knownFailure("imperative calls reject destroyed targets", last.live,
        "XF-LIFE-003", !last.live && last.op.op === "appendData", { duringRemoval, afterRemoval: last }));
    const registrations = bridge.registrations.getDiagnostics();
    checks.push(knownFailure("unmount releases widget registrations", registrations.mappingCount === 0 && registrations.tableCount === 0,
        "XF-LIFE-002", registrations.mappingCount === 2 && registrations.tableCount === 2, registrations));
  });

  await scenario("events before, during, and after deletion", async (bridge, fake, checks) => {
    let clicks = 0;
    const forwarded: { missing: boolean; unmounted: boolean }[] = [];
    let unmounted = false;
    const original = bridge.manager.dispatchEventFn!;
    bridge.manager.dispatchEventFn = (fiber, type, event) => {
        forwarded.push({ missing: fiber === undefined, unmounted });
        original(fiber, type, event);
    };
    await bridge.render(React.createElement("node", { root: true }, leaf("button", "button", () => clicks++)));
    const id = [...fake.nodes.values()].find(node => node.type === "di-button")!.id;
    bridge.manager.dispatchEvent(id, "onClick", { value: "clicked" });
    assert.equal(clicks, 1);
    const removing = bridge.render(null);
    bridge.manager.dispatchEvent(id, "onClick", { value: "during-delete" });
    await removing;
    const clicksAtUnmount = clicks;
    unmounted = true;
    bridge.manager.dispatchEvent(id, "onClick", { value: "late" });
    bridge.manager.dispatchEvent(999999, "onClick", { value: "unknown" });
    const lateCallbacks = clicks - clicksAtUnmount;
    checks.push(knownFailure("host drops missing/deleted event targets and their callbacks", forwarded.filter(item => item.unmounted).length === 0 && lateCallbacks === 0,
        "XF-LIFE-006", forwarded.filter(item => item.unmounted).length === 2 && forwarded.at(-1)!.missing && lateCallbacks === 1,
        { forwarded, lateCallbacks }));
  });

  await scenario("React cross-parent move remounts identity", async (bridge, fake, checks) => {
    const moved = (right: boolean) => React.createElement("node", { root: true },
        React.createElement("node", { key: "left" }, right ? null : leaf("moved")),
        React.createElement("node", { key: "right" }, right ? leaf("moved") : null));
    await bridge.render(moved(false));
    const old = [...fake.nodes.values()].find(node => node.type === "di-button")!.id;
    await bridge.render(moved(true));
    const current = [...fake.nodes.values()].filter(node => node.type === "di-button").at(-1)!.id;
    assert.notEqual(current, old);
    checks.push(knownFailure("cross-parent remount removes the old native child", !fake.nodes.has(old),
        "XF-LIFE-008", fake.nodes.has(old) && current !== old && fake.nodes.size === 6,
        { old, current, nodes: [...fake.nodes.values()] }));
    await bridge.render(null);
  });

  await scenario("Suspense fallback and abandoned work", async (bridge, fake, checks) => {
    const pending = new Promise<void>(() => {});
    function Suspend(): React.ReactNode { throw pending; }
    await bridge.render(React.createElement("node", { root: true },
        React.createElement(React.Suspense, { fallback: leaf("fallback") },
            React.createElement("node", null, leaf("prospective"), React.createElement(Suspend)))));
    check([...fake.nodes.values()].some(node => node.label === "fallback"), "Suspense fallback was not created");
    await bridge.render(null);
    const leaked = [...fake.nodes.values()].filter(node => node.id !== 0);
    checks.push(knownFailure("abandoned Suspense work leaves no native nodes", leaked.length === 0,
        "XF-LIFE-009", leaked.length === 1 && leaked[0].label === "prospective", leaked));
  });

  await scenario("Strict Mode effects remain executable", async (bridge, fake) => {
    let setups = 0, cleanups = 0;
    function StrictFixture() {
        useEffect(() => { setups++; return () => { cleanups++; }; }, []);
        return tree(["strict"]);
    }
    await bridge.render(React.createElement(React.StrictMode, null, React.createElement(StrictFixture)));
    await waitFor(() => setups, value => value > 0, "Strict Mode effect");
    await bridge.render(null);
    await waitFor(() => cleanups, value => value === setups, "effect cleanup");
    assert.equal(fake.nodes.size, 1);
  });
  console.log(`${mode}: ${results.length} bridge lifecycle scenarios passed; ${results.reduce((sum, result) => sum + result.checks.length, 0)} known defects reproduced`);
}

main().catch(error => { console.error(error); process.exitCode = 1; }).finally(save);
