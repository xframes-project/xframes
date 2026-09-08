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
    assert.equal(state.bridge.fiberCount, 0, "XF-LIFE-001: unmount releases every Fiber mapping");
    assert.equal(state.registrations.nativeCount, 0);
    assert.equal(state.registrations.mappingCount, 0);
    assert.equal(state.registrations.reverseMappingCount, 0);
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
    assert.equal(rootChildren.length, 1);
    assert.equal(typeof rootChildren[0], "number", "XF-LIFE-007: native identity is numeric");
    assert.equal(bridge.registrations.captureWidget("public-root"), undefined);
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
    const operationCount = fake.internalOps.length;
    retainedHandle.appendData(3, 6);
    assert.equal(fake.internalOps.length, operationCount, "XF-LIFE-003: stale handle is a no-op");
    const registrations = bridge.registrations.getDiagnostics();
    assert.equal(registrations.mappingCount, 0);
    assert.equal(registrations.tableCount, 0, "XF-LIFE-002: effect registrations released");
    assert.equal(registrations.registrationCount, 0);
    assert.equal(registrations.droppedOperations, 1);
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
    assert.equal(forwarded.filter(item => item.unmounted).length, 0);
    assert.equal(lateCallbacks, 0, "XF-LIFE-006: late events cannot call deleted widgets");
    assert.equal(bridge.manager.getDiagnostics().droppedEvents, 2);
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
    // No destruction was acknowledged for the abandoned node. Stage 1 must not
    // disguise speculative publication with an unrelated global registry sweep.
    assert.equal(bridge.manager.getDiagnostics().fiberCount, 1);
    assert.equal(bridge.registrations.getDiagnostics().nativeCount, 1);
    assert.equal(bridge.registrations.getDiagnostics().mappingCount, 1);
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
  await scenario("public ID mutation, reuse, and registration ownership", async (bridge, fake) => {
    const root = (id?: string, key = "owner") => React.createElement("node", { root: true, id, key });
    await bridge.render(root());
    const nativeId = fake.children.get(0)![0];
    await bridge.render(root("first"));
    const old = bridge.registrations.captureWidget("first")!;
    assert.equal(old.nativeId, nativeId);
    const cleanup = bridge.registrations.registerTable(old);
    await bridge.render(root("second"));
    assert.equal(bridge.registrations.captureWidget("first"), undefined);
    assert.equal(bridge.registrations.captureWidget("second"), old);
    assert.equal(bridge.registrations.getDiagnostics().tableCount, 1);
    await bridge.render(root("second", "replacement"));
    const current = bridge.registrations.captureWidget("second")!;
    assert.notEqual(current.nativeId, old.nativeId);
    bridge.registrations.registerTable(current);
    cleanup(); cleanup();
    bridge.registrations.unregisterTable(old);
    bridge.registrations.unlinkWidgetIds("second", old);
    bridge.registrations.registerTable(old); // delayed setup for the dead owner
    assert.equal(bridge.registrations.getDiagnostics().tableCount, 1);
    assert.equal(bridge.registrations.getDiagnostics().registrationCount, 1);
    bridge.registrations.setTableData(old, [{ value: "stale" }]);
    assert.equal(fake.internalOps.length, 0);
    bridge.registrations.setTableData(current, [{ value: "live" }]);
    assert.equal(fake.internalOps.at(-1)!.id, current.nativeId);
    assert.equal(fake.internalOps.at(-1)!.live, true);
    await bridge.render(null);
    assert.equal(bridge.registrations.getDiagnostics().nativeCount, 0);
  });

  await scenario("delayed and duplicate native acknowledgments, queued events, disposal", async (bridge, fake) => {
    let clicks = 0;
    await bridge.render(React.createElement("node", { root: true }, leaf("late", "late", () => clicks++)));
    const old = bridge.registrations.captureWidget("late")!;
    const cleanup = bridge.registrations.registerMap(old);
    const queuedEvent = () => bridge.manager.dispatchEvent(old.nativeId, "onClick", {});
    queuedEvent();
    assert.equal(clicks, 1);
    fake.delivery.delayed = true;
    await bridge.render(null);
    assert.equal(fake.nodes.size, 1);
    assert.ok(fake.pendingDestructions.length);
    queuedEvent(); // Native is dead even though its result has not been delivered.
    bridge.registrations.setMapMarkers(old, []);
    assert.equal(clicks, 1);
    assert.equal(fake.internalOps.length, 0);
    await bridge.render(React.createElement("node", { root: true }, leaf("late", "new", () => clicks++)));
    const current = bridge.registrations.captureWidget("late")!;
    bridge.registrations.registerMap(current);
    for (const ids of fake.pendingDestructions.splice(0)) {
        bridge.manager.acknowledgeDestruction(ids);
        bridge.manager.acknowledgeDestruction(ids);
    }
    cleanup();
    assert.equal(bridge.registrations.captureWidget("late"), current);
    bridge.manager.dispatchEvent(current.nativeId, "onClick", {});
    assert.equal(clicks, 2);
    await bridge.render(null);
    bridge.manager.destroy();
    for (const ids of fake.pendingDestructions.splice(0)) bridge.manager.acknowledgeDestruction(ids);
    queuedEvent();
    assert.equal(clicks, 2);
    assert.equal(bridge.manager.wasmModule, undefined);
    assert.equal(bridge.manager.dispatchEventFn, undefined);
    assert.equal(bridge.manager.getDiagnostics().fiberCount, 0);
    assert.equal(bridge.registrations.getDiagnostics().registrationCount, 0);
  });

  await scenario("all component registrations and Strict Mode saved handles", async (bridge, fake) => {
    const widgets = [components.Table, components.PlotBar, components.PlotLine, components.PlotCandlestick,
        components.PlotScatter, components.PlotHeatmap, components.PlotHistogram, components.PlotPieChart,
        components.MapView, components.Image, components.ClippedMultiLineTextRenderer,
        components.JsCanvas, components.LuaCanvas, components.JanetCanvas, components.Combo,
        components.InputText, components.Slider];
    const refs = widgets.map(() => React.createRef<any>());
    await bridge.render(React.createElement(React.StrictMode, null, React.createElement("node", { root: true },
        widgets.map((Widget, index) => React.createElement(Widget as React.ComponentType<any>, {
            key: index, ref: refs[index], columns: [], options: [],
        })))));
    await waitFor(() => bridge.registrations.getDiagnostics().registrationCount,
        value => value === widgets.length, "every widget has one lifetime registration");
    const handles = refs.map(ref => ref.current);
    handles[0].setTableData([{ value: 5 }]);
    handles[1].appendData(1, 2);
    handles[8].setMarkers([]);
    handles[11].setData({ value: 1 });
    assert.equal(fake.internalOps.length, 4);
    assert.equal(fake.internalOps.every(op => op.live), true);
    await bridge.render(null);
    assert.equal(bridge.registrations.getDiagnostics().registrationCount, 0);
    handles[0].setTableData([]);
    handles[1].appendData(2, 3);
    handles[8].setMarkers([]);
    handles[11].setData({ value: 2 });
    assert.equal(fake.internalOps.length, 4);
    assert.equal(bridge.registrations.getDiagnostics().droppedOperations, 4);
  });

  await scenario("registration lease duplicate cleanup and real error propagation", async (bridge, fake) => {
    fake.binding.setElement(JSON.stringify({ id: 100001, type: "plot-bar" }));
    bridge.registrations.linkWidgetIds("direct-caller", 100001);
    assert.equal(bridge.registrations.captureWidget("direct-caller")!.nativeId, 100001);
    fake.binding.setChildren(0, JSON.stringify([100001]));
    bridge.manager.acknowledgeDestruction(JSON.parse(fake.binding.setChildren(0, "[]")));
    bridge.registrations.linkWidgetIds("late-direct-caller", 100001);
    assert.equal(bridge.registrations.captureWidget("late-direct-caller"), undefined);
    await bridge.render(tree(["owner"]));
    const owner = bridge.registrations.captureWidget("owner")!;
    const first = bridge.registrations.registerTable(owner);
    first();
    const second = bridge.registrations.registerTable(owner);
    first();
    assert.equal(bridge.registrations.getDiagnostics().tableCount, 1);
    const error = new Error("native validation failure");
    fake.binding.elementInternalOp = () => { throw error; };
    assert.throws(() => bridge.registrations.setTableData(owner, []), value => value === error);
    second();
    assert.equal(bridge.registrations.getDiagnostics().registrationCount, 0);
    await bridge.render(null);
  });
  await scenario("bounded Wasm event queue dispatches after native locks and disposes payloads", async (bridge, fake) => {
    let clicks = 0;
    await bridge.render(React.createElement("node", { root: true }, leaf("queued", "queued", () => clicks++)));
    const id = bridge.registrations.captureWidget("queued")!.nativeId;
    bridge.manager.enqueueEvent(id, "onClick", {});
    assert.equal(clicks, 0);
    await Promise.resolve();
    assert.equal(clicks, 1);
    bridge.manager.enqueueEvent(id, "onClick", {});
    const destroyed = JSON.parse(fake.binding.setChildren(0, "[]"));
    bridge.manager.acknowledgeDestruction(destroyed);
    await Promise.resolve();
    assert.equal(clicks, 1);
    for (let i = 0; i < 300; i++) bridge.manager.enqueueEvent(999999, "onClick", { i });
    assert.equal(bridge.manager.getDiagnostics().pendingEventCount, 256);
    await Promise.resolve();
    assert.equal(bridge.manager.getDiagnostics().pendingEventCount, 0);
    assert.equal(bridge.manager.getDiagnostics().droppedEvents, 301);
    bridge.manager.enqueueEvent(id, "onClick", { queuedAtDisposal: true });
    await bridge.render(null);
    bridge.manager.enqueueEvent(id, "onClick", {});
    bridge.manager.destroy();
    assert.equal(bridge.manager.getDiagnostics().pendingEventCount, 0);
    await Promise.resolve();
    assert.equal(clicks, 1);
  });
  await scenario("multiple populated roots, partial removal, replacement, repeated empty unmount", async (bridge, fake) => {
    const root = (id: string) => React.createElement("node", { root: true, key: id, id }, leaf(`${id}-child`));
    await bridge.render([root("left"), root("right")]);
    const right = bridge.registrations.captureWidget("right")!;
    const child = bridge.registrations.captureWidget("right-child")!;
    assert.equal(fake.children.get(0)!.length, 2);
    await bridge.render([root("right"), root("left")]);
    assert.equal(fake.children.get(0)![0], right.nativeId);
    await bridge.render([root("right")]);
    assert.equal(fake.nodes.size, 3);
    assert.equal(bridge.registrations.captureWidget("right"), right);
    assert.equal(bridge.registrations.captureWidget("right-child"), child);
    assert.equal(bridge.registrations.captureWidget("left"), undefined);
    await bridge.render([root("replacement")]);
    assert.equal(right.alive, false);
    assert.equal(child.alive, false);
    await bridge.render(null);
    await bridge.render(null);
    assert.equal(fake.nodes.size, 1);
    assert.equal(bridge.manager.getDiagnostics().fiberCount, 0);
    assert.equal(bridge.registrations.getDiagnostics().nativeCount, 0);
  });
  console.log(`${mode}: ${results.length} bridge lifecycle scenarios passed; ${results.reduce((sum, result) => sum + result.checks.length, 0)} known defects reproduced`);
}

main().catch(error => { console.error(error); process.exitCode = 1; }).finally(save);
