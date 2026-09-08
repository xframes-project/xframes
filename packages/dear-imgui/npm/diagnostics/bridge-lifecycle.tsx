import assert from "node:assert/strict";
import { mkdirSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import React, { useEffect, useState } from "react";
import { createBridge, createFakeBinding } from "./bridge";
import { components } from "@xframes/common";
import { check, waitFor, type InvariantResult } from "./assertions";

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
        await bridge.dispose();
        assert.equal(bridge.manager.getDiagnostics().subscriptionClosed, true);
        checks.push({ name: "bridge disposal releases its subscription", status: "pass", defect: "XF-LIFE-010" });
        assert.deepEqual(bridge.rendererErrors, [], "No unaccounted React commit errors");
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
    const earlyCalls = trace.slice(0, firstCommit).filter(call => call.method === "applyCommit").length;
    assert.equal(earlyCalls, 0, "XF-LIFE-004: prospective work makes no native structural calls");
    assert.equal(state.bridge.publications, state.bridge.structuralCalls);
    assert.equal(state.bridge.publications, state.bridge.appliedPublications);
    assert.equal(BigInt(state.bridge.nativeRevision!), BigInt(state.bridge.appliedPublications));
    assert.equal(state.bridge.committedDescriptionCount, 0);
    assert.equal(state.bridge.retainedCandidateCount, 0);
    checks.push({ name: "unmount releases native and Fiber mappings", status: "pass", defect: "XF-LIFE-001" });
    checks.push({ name: "prospective work stays out of native state", status: "pass", defect: "XF-LIFE-004" });
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
    check(bridge.manager.getDiagnostics().appliedPublications > 1, "No native prop publication observed");
    await bridge.render(null);
  });

  await scenario("callback-only updates publish committed event props", async (bridge, fake) => {
    const calls: string[] = [];
    const render = (callback?: () => void) => bridge.render(React.createElement("node", { root: true }, leaf("event", "unchanged", callback)));
    await render(() => calls.push("first"));
    const target = bridge.registrations.captureWidget("event")!;
    const revision = BigInt(bridge.manager.getDiagnostics().nativeRevision!);
    bridge.manager.dispatchEvent(target.nativeId, "onClick", {});
    await render(() => calls.push("second"));
    assert.equal(bridge.registrations.captureWidget("event"), target);
    assert.equal(BigInt(bridge.manager.getDiagnostics().nativeRevision!), revision + 1n);
    assert.equal(fake.nodes.get(target.nativeId)!.label, "unchanged");
    bridge.manager.dispatchEvent(target.nativeId, "onClick", {});
    await render();
    bridge.manager.dispatchEvent(target.nativeId, "onClick", {});
    assert.deepEqual(calls, ["first", "second"]);
  });

  await scenario("independent prospective clones, same-ID move and abandoned ID reuse", async (bridge, fake, checks) => {
    let clicks = 0;
    await bridge.render(React.createElement("node", { root: true, id: "root" },
        React.createElement("node", { id: "left" }, leaf("survivor", "original", () => clicks++)),
        React.createElement("node", { id: "right" })));
    const manager = bridge.manager;
    const original = manager.fiberNodesMap.get(fake.children.get(0)![0]).stateNode.node;
    const [left, right] = original.children;
    const survivor = left.children[0];
    const target = bridge.registrations.captureWidget("survivor")!;
    const lease = bridge.registrations.registerTable(target);
    const before = JSON.stringify([...fake.nodes]);
    const publications = manager.getDiagnostics().publications;
    assert.equal(manager.cloneNodeWithNewProps(original, { root: null }).props.root, true);
    assert.throws(() => manager.cloneNodeWithNewProps(original, { root: false }), /Root metadata/);
    assert.throws(() => manager.cloneNodeWithNewProps(original, { root: "true" }), /Root metadata must be boolean/);
    const branchA = manager.cloneNodeWithNewProps(survivor, { label: "abandoned", id: "stolen" });
    const branchB = manager.cloneNodeWithNewProps(survivor, { label: "committed" });
    const abandoned = manager.cloneNodeWithNewChildren(original);
    manager.appendChild(abandoned, branchA);
    for (let i = 0; i < 1000; i++) {
        const ignored = manager.createNode(1000000 + i, "di-button", 0, { id: "survivor", label: "discarded" }, {});
        const attempt = manager.cloneNodeWithNewChildren(original);
        manager.appendChild(attempt, ignored);
    }
    assert.equal(JSON.stringify([...fake.nodes]), before);
    assert.equal(manager.getDiagnostics().publications, publications);
    assert.equal(manager.getDiagnostics().retainedCandidateCount, 0);
    assert.equal(bridge.registrations.captureWidget("survivor"), target);
    assert.equal(bridge.registrations.captureWidget("stolen"), undefined);
    assert.equal(manager.fiberNodesMap.get(survivor.id).stateNode.node, survivor);
    manager.dispatchEvent(survivor.id, "onClick", {});
    bridge.registrations.setTableData(target, [{ value: "pending" }]);
    assert.equal(clicks, 1);
    assert.equal(fake.internalOps.at(-1)!.live, true);
    assert.equal(branchA.props.label, "abandoned");
    assert.equal(branchB.props.label, "committed");
    assert.equal(survivor.props.label, "original");
    const movedRight = manager.cloneNodeWithNewChildren(right);
    manager.appendChild(movedRight, branchB);
    const finalRoot = manager.cloneNodeWithNewChildren(original);
    manager.appendChild(finalRoot, movedRight); // left disappears; its child survives under right.
    const set = manager.createChildSet();
    manager.appendChildToSet(set, finalRoot);
    manager.completeRoot(0, set);
    assert.equal(fake.nodes.has(left.id), false);
    assert.equal(fake.nodes.get(survivor.id)!.label, "committed");
    assert.deepEqual(fake.children.get(right.id), [survivor.id]);
    assert.equal(bridge.registrations.captureWidget("survivor"), target);
    assert.equal(bridge.registrations.getDiagnostics().registrationCount, 1);
    manager.dispatchEvent(survivor.id, "onClick", {});
    assert.equal(clicks, 2);
    assert.throws(() => manager.appendChild(finalRoot, branchA), TypeError, "Published children are immutable");
    assert.throws(() => manager.createChildSet(1), /Unsupported Fabric surface/);
    assert.throws(() => manager.createNode(999999, "node", 1, {}, {}), /Unsupported Fabric surface/);
    assert.throws(() => manager.completeRoot(1, manager.createChildSet()), /Unsupported Fabric surface/);
    assert.equal(manager.getDiagnostics().publications, publications + 1);
    manager.completeRoot(0, manager.createChildSet());
    assert.equal(target.alive, false);
    assert.equal(bridge.registrations.getDiagnostics().registrationCount, 0);
    lease();
    checks.push({ name: "one-publication same-ID move preserves the JS lifetime", status: "pass", defect: "XF-LIFE-005" });
  });

  await scenario("Suspense transition keeps committed callbacks and IDs until reveal", async (bridge, fake) => {
    let resume!: () => void;
    let ready = false, attempts = 0;
    const gate = new Promise<void>(resolve => { resume = () => { ready = true; resolve(); }; });
    const calls: string[] = [];
    function Suspend() { attempts++; if (!ready) throw gate; return null; }
    const fixture = (pending: boolean) => React.createElement("node", { root: true },
        React.createElement(React.Suspense, { fallback: leaf("fallback") },
            React.createElement("node", null,
                React.createElement("di-button", { key: "stable", id: pending ? "renamed" : "live", label: pending ? "next" : "old",
                    onClick: pending ? () => calls.push("new") : () => calls.push("old") }),
                pending && leaf("live", "prospective thief"), pending && React.createElement(Suspend))));
    await bridge.render(fixture(false));
    const live = bridge.registrations.captureWidget("live")!;
    const lease = bridge.registrations.registerTable(live);
    const baseline = bridge.manager.getDiagnostics();
    let completed = false;
    let transition!: Promise<void>;
    React.startTransition(() => { transition = bridge.render(fixture(true)).then(() => { completed = true; }); });
    await waitFor(() => attempts, count => count > 0, "transition suspended after prospective host completion");
    assert.ok(bridge.manager.getDiagnostics().observedClones > baseline.observedClones);
    assert.equal(bridge.manager.getDiagnostics().publications, baseline.publications);
    assert.equal(completed, false);
    assert.equal(fake.nodes.get(live.nativeId)!.label, "old");
    assert.equal(bridge.registrations.captureWidget("live"), live);
    assert.equal(bridge.registrations.captureWidget("renamed"), undefined);
    bridge.manager.dispatchEvent(live.nativeId, "onClick", {});
    bridge.registrations.setTableData(live, [{ value: "still committed" }]);
    assert.deepEqual(calls, ["old"]);
    assert.equal(fake.internalOps.at(-1)!.live, true);
    resume();
    await transition;
    assert.equal(bridge.registrations.captureWidget("renamed"), live);
    assert.notEqual(bridge.registrations.captureWidget("live"), live);
    assert.equal(bridge.registrations.getDiagnostics().registrationCount, 1);
    bridge.manager.dispatchEvent(live.nativeId, "onClick", {});
    assert.deepEqual(calls, ["old", "new"]);
    assert.equal(fake.nodes.get(live.nativeId)!.label, "next");
    lease();
  });

  for (const failure of ["rejected", "failed", "invalid-acknowledgment"] as const) {
    await scenario(`real Fabric ${failure} prevents success and invalidates handles`, async (bridge, fake) => {
      await bridge.render(tree(["old"]));
      const target = bridge.registrations.captureWidget("old")!;
      bridge.registrations.registerTable(target);
      const before = JSON.stringify([...fake.nodes]);
      const revision = bridge.manager.getDiagnostics().nativeRevision;
      const apply = fake.binding.applyCommit;
      fake.binding.applyCommit = wire => {
        if (failure === "invalid-acknowledgment") {
          const result = JSON.parse(apply(wire));
          result.destroyedIds = [];
          return JSON.stringify(result);
        }
        return JSON.stringify({ schemaVersion: 2, surfaceId: 0, status: failure, nativeRevision: revision,
            nativeSequence: failure === "rejected" ? null : String(BigInt(revision!) + 1n), destroyedIds: [],
            error: { code: failure === "rejected" ? "invalid_props" : "application_error", operationIndex: 0, message: "injected" } });
      };
      let success = false;
      await assert.rejects(bridge.render(tree(["new"])).then(() => { success = true; }), /Native publication/);
      assert.equal(success, false);
      assert.equal(target.alive, false);
      assert.equal(bridge.registrations.captureWidget("new"), undefined);
      assert.equal(bridge.registrations.getDiagnostics().nativeCount, 0);
      assert.equal(bridge.manager.getDiagnostics().committedDescriptionCount, 0);
      assert.equal(bridge.manager.getDiagnostics().failedPublications, 1);
      assert.equal(bridge.manager.getDiagnostics().lastPublication!.status, failure === "rejected" ? "rejected" : "failed");
      assert.equal(bridge.manager.getDiagnostics().stagingNodeCount, 0);
      assert.equal(bridge.manager.getDiagnostics().nativeRevision, revision);
      if (failure === "rejected") assert.equal(JSON.stringify([...fake.nodes]), before);
      bridge.registrations.setTableData(target, []);
      assert.equal(fake.internalOps.length, 0);
      assert.throws(() => bridge.manager.assertPublicationHealthy(), /Fabric surface failed/);
      await bridge.dispose();
      assert.ok(bridge.rendererErrors.length > 0);
      for (const error of bridge.rendererErrors.splice(0)) assert.match(String(error), /Native publication/);
    });
  }

  await scenario("removing a public ID preserves native identity", async (bridge, fake, checks) => {
    await bridge.render(React.createElement("node", { root: true, id: "public-root" }, leaf("a")));
    await bridge.render(React.createElement("node", { root: true }, leaf("a")));
    const rootChildren = fake.children.get(0)!;
    assert.equal(rootChildren.length, 1);
    assert.equal(typeof rootChildren[0], "number", "XF-LIFE-007: native identity is numeric");
    assert.equal(bridge.registrations.captureWidget("public-root"), undefined);
    checks.push({ name: "removing a public ID preserves numeric native identity", status: "pass", defect: "XF-LIFE-007" });
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
    checks.push({ name: "effect registrations are released", status: "pass", defect: "XF-LIFE-002" });
    checks.push({ name: "stale handles cannot reach a replacement", status: "pass", defect: "XF-LIFE-003" });
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
    checks.push({ name: "events cannot target a deleted lifetime", status: "pass", defect: "XF-LIFE-006" });
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
    assert.equal(fake.nodes.has(old), false, "XF-LIFE-008: cross-parent remount destroys old lifetime");
    assert.equal(bridge.manager.fiberNodesMap.has(old), false);
    assert.equal(fake.nodes.size, 5);
    checks.push({ name: "cross-parent remount removes old lifetime", status: "pass", defect: "XF-LIFE-008" });
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
    assert.equal(leaked.length, 0, "XF-LIFE-009: abandoned Suspense work never publishes");
    assert.equal(bridge.manager.getDiagnostics().fiberCount, 0);
    assert.equal(bridge.registrations.getDiagnostics().nativeCount, 0);
    assert.equal(bridge.registrations.getDiagnostics().mappingCount, 0);
    assert.equal(bridge.manager.getDiagnostics().committedDescriptionCount, 0);
    checks.push({ name: "abandoned Suspense work leaves no native nodes", status: "pass", defect: "XF-LIFE-009" });
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

  await scenario("duplicate destruction receipts after rebinding, queued events and disposal", async (bridge, fake) => {
    let clicks = 0;
    await bridge.render(React.createElement("node", { root: true }, leaf("late", "late", () => clicks++)));
    const old = bridge.registrations.captureWidget("late")!;
    const cleanup = bridge.registrations.registerMap(old);
    const queuedEvent = () => bridge.manager.dispatchEvent(old.nativeId, "onClick", {});
    queuedEvent();
    assert.equal(clicks, 1);
    const receipts: number[][] = [];
    const apply = fake.binding.applyCommit;
    fake.binding.applyCommit = (wire: string) => {
        const result = apply(wire);
        const ids = JSON.parse(result).destroyedIds;
        if (ids.length) receipts.push(ids);
        return result;
    };
    await bridge.render(null);
    assert.equal(fake.nodes.size, 1);
    assert.ok(receipts.length);
    queuedEvent(); // The actual publication acknowledgment has already invalidated this target.
    bridge.registrations.setMapMarkers(old, []);
    assert.equal(clicks, 1);
    assert.equal(fake.internalOps.length, 0);
    await bridge.render(React.createElement("node", { root: true }, leaf("late", "new", () => clicks++)));
    const current = bridge.registrations.captureWidget("late")!;
    bridge.registrations.registerMap(current);
    for (const ids of receipts.splice(0)) {
        bridge.manager.acknowledgeDestruction(ids);
        bridge.manager.acknowledgeDestruction(ids);
    }
    cleanup();
    assert.equal(bridge.registrations.captureWidget("late"), current);
    bridge.manager.dispatchEvent(current.nativeId, "onClick", {});
    assert.equal(clicks, 2);
    await bridge.render(null);
    bridge.manager.destroy();
    for (const ids of receipts.splice(0)) bridge.manager.acknowledgeDestruction(ids);
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
            key: index, ref: refs[index], columns: [{ fieldId: "value", heading: "Value" }], options: [],
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
    const direct = (rootChildren: number[], operations: unknown[]) => JSON.parse(fake.binding.applyCommit(JSON.stringify({
        schemaVersion: 2, surfaceId: 0, baseRevision: JSON.parse(fake.binding.getCommitState()).nativeRevision, rootChildren, operations,
    })));
    assert.equal(direct([100001], [{ op: "create", id: 100001, elementType: "plot-bar", props: {} },
        { op: "setChildren", parentId: 100001, childrenIds: [] }]).status, "applied");
    bridge.registrations.linkWidgetIds("direct-caller", 100001);
    assert.equal(bridge.registrations.captureWidget("direct-caller")!.nativeId, 100001);
    bridge.manager.acknowledgeDestruction(direct([], []).destroyedIds);
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
    const apply = fake.binding.applyCommit;
    fake.binding.applyCommit = (wire: string) => {
        const result = apply(wire);
        if (JSON.parse(result).destroyedIds.includes(id)) bridge.manager.enqueueEvent(id, "onClick", {});
        return result;
    };
    await bridge.render(null);
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
  await scenario("one publication per completeRoot and explicit stale-writer failure", async (bridge, fake) => {
    await bridge.render(tree(["transaction-owner"]));
    const initial = JSON.parse(fake.binding.getCommitState());
    assert.equal(BigInt(initial.nativeRevision), BigInt(bridge.observer.snapshot().counts.applyCommit));
    const owner = bridge.registrations.captureWidget("transaction-owner")!;
    const roots = fake.children.get(0)!;
    const rootNode = bridge.manager.fiberNodesMap.get(roots[0]).stateNode.node;
    const external = JSON.parse(fake.binding.applyCommit(JSON.stringify({ schemaVersion: 2, surfaceId: 0,
        baseRevision: initial.nativeRevision, rootChildren: roots,
        operations: [{ op: "patch", id: owner.nativeId, props: { label: "external" } },
            ...[...fake.children].filter(([id]) => id !== 0).map(([parentId, childrenIds]) => ({ op: "setChildren", parentId, childrenIds }))],
    })));
    assert.equal(external.status, "applied");
    const set = bridge.manager.createChildSet(0);
    bridge.manager.appendChildToSet(set, rootNode);
    assert.throws(() => bridge.manager.completeRoot(0, set), /stale_revision/);
    assert.equal(bridge.manager.getDiagnostics().failedPublications, 1);
    assert.equal(owner.alive, false);
    assert.equal(bridge.manager.getDiagnostics().fiberCount, 0);
    assert.equal(bridge.registrations.getDiagnostics().nativeCount, 0);
    assert.equal(fake.nodes.get(owner.nativeId)!.label, "external");
    assert.throws(() => bridge.manager.assertPublicationHealthy(), /stale_revision/);
  });
  const defects = new Set(results.flatMap(result => result.checks.map((check: InvariantResult) => check.defect)));
  for (let id = 1; id <= 10; id++) assert.ok(defects.has(`XF-LIFE-${String(id).padStart(3, "0")}`), `Missing defect gate ${id}`);
  console.log(`${mode}: ${results.length} bridge lifecycle scenarios passed; all 10 defect gates executed`);
}

main().catch(error => { console.error(error); process.exitCode = 1; }).finally(save);
