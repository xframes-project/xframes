import React from "react";
import type NativeFabricUIManager from "../common/src/lib/react-native/nativeFabricUiManager";
import { ReactNativePrivateInterface as privateInterface, ReactFabricInitialiser,
    WidgetRegistrationService, WidgetRegistrationServiceContext } from "@xframes/common";

export type NativeBinding = Record<string, (...args: any[]) => any>;
type FabricRenderer = {
    render(element: React.ReactNode, container: number, callback: () => void, concurrent: number, options: undefined): void;
    stopSurface(container: number): void;
};
export type CallRecord = { index: number; atMs: number; method: string; args: unknown[]; bytes: number };
const nativeMethods = ["setElement", "patchElement", "setChildren", "appendChild", "elementInternalOp", "isElementAlive", "applyCommit", "getCommitState"];

/** Observes actual calls, including calls before completeRoot. This is not a commit log. */
export function observeBinding(binding: NativeBinding, capacity = 256) {
    const trace: CallRecord[] = [];
    const counts: Record<string, number> = {};
    let bytes = 0;
    let index = 0;
    const encoder = new TextEncoder();
    const record = (method: string, args: unknown[]) => {
        const payloadBytes = args.reduce<number>((sum, arg) =>
            sum + (typeof arg === "string" ? encoder.encode(arg).length : 0), 0);
        bytes += payloadBytes;
        counts[method] = (counts[method] ?? 0) + 1;
        trace.push({ index: ++index, atMs: performance.now(), method, bytes: payloadBytes,
            args: args.map(arg => typeof arg === "string" && arg.length > 2048
                ? { prefix: arg.slice(0, 2048), omittedCharacters: arg.length - 2048 } : arg) });
        if (trace.length > capacity) trace.shift();
    };
    const observed = Object.create(binding) as NativeBinding;
    for (const method of nativeMethods) {
        if (typeof binding[method] !== "function") continue;
        observed[method] = (...args: unknown[]) => {
            record(method, args);
            return binding[method](...args);
        };
    }
    return { binding: observed, record, snapshot: () => ({
        counts: { ...counts }, serializedBytes: bytes, totalRecords: index,
        droppedRecords: Math.max(0, index - trace.length), trace: [...trace],
    }) };
}

export function createBridge(binding: NativeBinding) {
    const Manager = privateInterface.nativeFabricUIManager.constructor as new () => NativeFabricUIManager;
    const manager = new Manager();
    const observer = observeBinding(binding);
    const registrations = new WidgetRegistrationService(observer.binding);
    const host = Object.create(privateInterface);
    Object.defineProperty(host, "nativeFabricUIManager", { value: manager });
    // Source and built declarations have distinct private-field nominal identities.
    manager.init(observer.binding, registrations as unknown as Parameters<NativeFabricUIManager["init"]>[1]);
    const originalCompleteRoot = manager.completeRoot;
    manager.completeRoot = (container, children) => {
        observer.record("completeRoot:enter", [container, children.map((node: any) => node.id)]);
        originalCompleteRoot(container, children);
        observer.record("completeRoot:exit", [container]);
    };
    const renderer = ReactFabricInitialiser(host) as FabricRenderer;
    const render = (element: React.ReactNode): Promise<void> => new Promise((resolve, reject) => {
        const timeout = setTimeout(() => reject(new Error("Fabric completion timed out")), 10_000);
        renderer.render(
            React.createElement(WidgetRegistrationServiceContext.Provider, { value: registrations }, element),
            0,
            () => { clearTimeout(timeout); resolve(); },
            1,
            undefined,
        );
    });
    return { manager, registrations, renderer, observer, render,
        snapshot: () => ({ bridge: manager.getDiagnostics(), registrations: registrations.getDiagnostics(),
            operations: observer.snapshot() }),
        dispose: () => { renderer.stopSurface(0); manager.destroy(); },
    };
}

/** Minimal binding double for bridge tests only; native parity is tested in C++. */
export function createFakeBinding() {
    const nodes = new Map<number, Record<string, any>>([[0, { id: 0, type: "container" }]]);
    const children = new Map<number, number[]>([[0, []]]);
    const internalOps: { id: number; live: boolean; op: any }[] = [];
    const pendingDestructions: number[][] = [];
    const delivery = { delayed: false };
    let nativeSequence = 0n;
    let nativeRevision = 0n;
    const state = () => ({ schemaVersion: 1, surfaceId: 0, initialized: true,
        nativeSequence: String(nativeSequence), nativeRevision: String(nativeRevision) });
    // The double stages plain JS values for preflight; actual queue/locking/props
    // behavior is covered by native and shared real-binding fixtures.
    const apply = (wire: string, compatibility = false) => {
        let operationIndex: number | null = null;
        const rejected = (code: string) => JSON.stringify({ ...state(), status: "rejected", nativeSequence: null,
            destroyedIds: [], error: { code, message: code, operationIndex } });
        let batch: any;
        try { batch = JSON.parse(wire); } catch { return rejected("invalid_json"); }
        if (batch.schemaVersion !== 1) return rejected("unsupported_version");
        if (batch.surfaceId !== 0) return rejected("unsupported_surface");
        if (!Array.isArray(batch.operations)) return rejected("invalid_field");
        const nextNodes = new Map([...nodes].map(([id, node]) => [id, { ...node }]));
        const nextChildren = new Map([...children].map(([id, ids]) => [id, [...ids]]));
        const destroyedIds: number[] = [];
        const remove = (id: number) => {
            for (const child of nextChildren.get(id) ?? []) remove(child);
            if (nextNodes.delete(id)) destroyedIds.push(id);
            nextChildren.delete(id);
        };
        for (let i = 0; i < batch.operations.length; i++) {
            operationIndex = i;
            const op = batch.operations[i];
            if (!op || !["create", "patch", "setChildren", "appendChild"].includes(op.op)) return rejected("unsupported_operation");
            const target = op.id ?? op.parentId;
            if (!Number.isInteger(target) || target < (op.id !== undefined ? 1 : 0) || target > 2147483647) return rejected("invalid_id");
            if (op.op === "create") {
                if (nextNodes.has(target)) return rejected("duplicate_id");
                if (destroyedIds.includes(target)) return rejected("destroyed_id");
                if (typeof op.elementType !== "string") return rejected("invalid_element_type");
                nextNodes.set(target, { ...op.props, id: target, type: op.elementType });
                nextChildren.set(target, []);
            } else {
                if (!nextNodes.has(target)) {
                    if (compatibility) continue;
                    return rejected("missing_target");
                }
                if (op.op === "patch") nextNodes.set(target, { ...nextNodes.get(target), ...op.props, id: target });
                else {
                    const siblings = nextChildren.get(target)!;
                    const next = op.op === "setChildren" ? op.childrenIds : siblings.includes(op.childId) ? siblings : [...siblings, op.childId];
                    if (!Array.isArray(next)) return rejected("invalid_field");
                    if (!compatibility && next.some((id: number) => !nextNodes.has(id))) return rejected("missing_target");
                    if (op.op === "setChildren") for (const old of siblings) if (!next.includes(old)) remove(old);
                    nextChildren.set(target, [...next]);
                }
            }
        }
        nodes.clear(); for (const entry of nextNodes) nodes.set(...entry);
        children.clear(); for (const entry of nextChildren) children.set(...entry);
        ++nativeSequence; ++nativeRevision;
        return JSON.stringify({ ...state(), status: "applied", destroyedIds,
            ...(batch.correlationId !== undefined ? { correlationId: batch.correlationId } : {}) });
    };
    const legacy = (op: any) => {
        const result = JSON.parse(apply(JSON.stringify({ schemaVersion: 1, surfaceId: 0, operations: [op] }), true));
        if (result.status !== "applied") throw new Error(JSON.stringify(result.error));
        return result;
    };
    const binding: NativeBinding = {
        applyCommit: (wire: string) => apply(wire),
        getCommitState: () => JSON.stringify(state()),
        setElement: (payload: string) => {
            const { id, type, ...props } = JSON.parse(payload);
            legacy({ op: "create", id, elementType: type, props });
        },
        patchElement: (id: number, payload: string) => {
            const { id: _id, type: _type, ...props } = JSON.parse(payload);
            legacy({ op: "patch", id, props });
        },
        setChildren: (id: number, payload: string) => {
            const { destroyedIds } = legacy({ op: "setChildren", parentId: id, childrenIds: JSON.parse(payload) });
            if (delivery.delayed) {
                if (destroyedIds.length) pendingDestructions.push(destroyedIds);
                return "[]";
            }
            return JSON.stringify(destroyedIds);
        },
        isElementAlive: (id: number) => id !== 0 && nodes.has(id),
        appendChild: (parentId: number, childId: number) => { legacy({ op: "appendChild", parentId, childId }); },
        elementInternalOp: (id: number, payload: string) => {
            internalOps.push({ id, live: nodes.has(id), op: JSON.parse(payload) });
            if (internalOps.length > 256) internalOps.shift();
        },
    };
    return { binding, nodes, children, internalOps, delivery, pendingDestructions };
}
