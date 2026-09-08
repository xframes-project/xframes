import React from "react";
import type NativeFabricUIManager from "../common/src/lib/react-native/nativeFabricUiManager";
import { ReactNativePrivateInterface as privateInterface, ReactFabricInitialiser,
    WidgetRegistrationService, WidgetRegistrationServiceContext } from "@xframes/common";

export type NativeBinding = Record<string, (...args: any[]) => any>;
type FabricRenderer = {
    render(element: React.ReactNode, container: number, callback: () => void, concurrent: number,
        options?: { onUncaughtError(error: unknown): void }): void;
    stopSurface(container: number): void;
};
export type CallRecord = { index: number; atMs: number; method: string; args: unknown[]; bytes: number };
const nativeMethods = ["elementInternalOp", "isElementAlive", "applyCommit", "getCommitState"];

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
    const rendererErrors: unknown[] = [];
    const pendingRenders = new Set<(error: unknown) => void>();
    const options = { onUncaughtError(error: unknown) {
        rendererErrors.push(error);
        for (const reject of [...pendingRenders]) reject(error);
    } };
    const render = (element: React.ReactNode): Promise<void> => new Promise((resolve, reject) => {
        const fail = (error: unknown) => { clearTimeout(timeout); pendingRenders.delete(fail); reject(error); };
        const timeout = setTimeout(() => fail(new Error("Fabric completion timed out")), 10_000);
        pendingRenders.add(fail);
        renderer.render(
            element === null ? null : React.createElement(WidgetRegistrationServiceContext.Provider, { value: registrations }, element),
            0,
            () => {
                clearTimeout(timeout);
                pendingRenders.delete(fail);
                try { manager.assertPublicationHealthy(); resolve(); } catch (error) { reject(error); }
            },
            1,
            options,
        );
    });
    let disposal: Promise<void> | undefined;
    return { manager, registrations, renderer, observer, render, rendererErrors,
        snapshot: () => ({ bridge: manager.getDiagnostics(), registrations: registrations.getDiagnostics(),
            operations: observer.snapshot() }),
        // Disposal releases the renderer even after terminal publication failure.
        // It deliberately does not signal an acknowledged native unmount.
        dispose: () => disposal ??= new Promise<void>((resolve, reject) => {
            const timeout = setTimeout(() => reject(new Error("Fabric disposal timed out")), 10_000);
            renderer.render(null, 0, () => {
                clearTimeout(timeout);
                renderer.stopSurface(0);
                manager.destroy();
                resolve();
            }, 1, options);
        }),
    };
}

/** Prospective/final-tree binding double. C++ is authoritative for props, Yoga, locks and resources. */
export function createFakeBinding() {
    const nodes = new Map<number, Record<string, any>>([[0, { id: 0, type: "container" }]]);
    const children = new Map<number, number[]>([[0, []]]);
    const internalOps: { id: number; live: boolean; op: any }[] = [];
    let nativeSequence = 0n, nativeRevision = 0n;
    const state = () => ({ schemaVersion: 2, surfaceId: 0, initialized: true, surfaceStatus: "healthy",
        managedCount: nodes.size - 1, nativeSequence: String(nativeSequence), nativeRevision: String(nativeRevision) });
    const apply = (wire: string) => {
        let operationIndex: number | null = null;
        let batch: any;
        const result = (status: string, destroyedIds: number[], code?: string) => JSON.stringify({
            schemaVersion: 2, surfaceId: 0, status, destroyedIds,
            nativeSequence: status === "rejected" ? null : String(nativeSequence), nativeRevision: String(nativeRevision),
            ...(batch?.correlationId === undefined ? {} : { correlationId: batch.correlationId }),
            ...(code ? { error: { code, message: code, operationIndex } } : {}),
        });
        const reject = (code: string) => result("rejected", [], code);
        try { batch = JSON.parse(wire); } catch { return reject("invalid_json"); }
        if (batch.schemaVersion !== 2) return reject("unsupported_version");
        if (batch.surfaceId !== 0) return reject("unsupported_surface");
        if (batch.baseRevision !== String(nativeRevision)) return reject("stale_revision");
        if (!Array.isArray(batch.operations) || !Array.isArray(batch.rootChildren)) return reject("invalid_field");
        const nextNodes = new Map([...nodes].map(([id, node]) => [id, { ...node }]));
        const nextChildren = new Map<number, number[]>([[0, [...batch.rootChildren]]]);
        for (let i = 0; i < batch.operations.length; i++) {
            operationIndex = i;
            const op = batch.operations[i];
            if (!op || !["create", "patch", "setChildren"].includes(op.op)) return reject("unsupported_operation");
            const id = op.id ?? op.parentId;
            if (!Number.isInteger(id) || id < 1 || id > 2147483647) return reject("invalid_id");
            if (op.op === "create") {
                if (nextNodes.has(id)) return reject("duplicate_id");
                if (typeof op.elementType !== "string") return reject("invalid_element_type");
                nextNodes.set(id, { ...op.props, id, type: op.elementType });
            }
        }
        for (let i = 0; i < batch.operations.length; i++) {
            operationIndex = i;
            const op = batch.operations[i], id = op.id ?? op.parentId;
            if (!nextNodes.has(id)) return reject("missing_target");
            if (op.op === "patch") {
                if ("id" in op.props || "type" in op.props || "root" in op.props) return reject("immutable_identity");
                nextNodes.set(id, { ...nextNodes.get(id), ...op.props });
            } else if (op.op === "setChildren") {
                if (!Array.isArray(op.childrenIds)) return reject("invalid_field");
                if (nextChildren.has(id)) return reject("duplicate_assignment");
                nextChildren.set(id, [...op.childrenIds]);
            }
        }
        const owners = new Map<number, number>();
        for (const [parent, list] of nextChildren) {
            const unique = new Set<number>();
            for (const id of list) {
                if (unique.has(id)) return reject("duplicate_child");
                unique.add(id);
                if (!nextNodes.has(id)) return reject("missing_target");
                if (!nextChildren.has(id)) return reject("missing_children");
                if (owners.has(id)) return reject("multiple_parents");
                if (id === parent) return reject("cycle");
                owners.set(id, parent);
            }
        }
        const reachable = new Set<number>([0]);
        const pending = [...batch.rootChildren];
        while (pending.length) {
            const id = pending.pop()!;
            if (reachable.has(id)) return reject("cycle");
            reachable.add(id); pending.push(...nextChildren.get(id)!);
        }
        for (const op of batch.operations) if (!reachable.has(op.id ?? op.parentId)) return reject("unreachable_operation");
        const destroyedIds: number[] = [];
        const destroy = (id: number) => {
            for (const child of children.get(id) ?? []) destroy(child);
            if (!reachable.has(id)) destroyedIds.push(id);
        };
        for (const id of children.get(0)!) destroy(id);
        for (const id of nextNodes.keys()) if (!reachable.has(id)) nextNodes.delete(id);
        nodes.clear(); for (const entry of nextNodes) nodes.set(...entry);
        children.clear(); for (const entry of nextChildren) children.set(...entry);
        ++nativeSequence; ++nativeRevision;
        return result("applied", destroyedIds);
    };
    const binding: NativeBinding = {
        applyCommit: apply,
        getCommitState: () => JSON.stringify(state()),
        isElementAlive: (id: number) => id !== 0 && nodes.has(id),
        elementInternalOp: (id: number, payload: string) => {
            internalOps.push({ id, live: nodes.has(id), op: JSON.parse(payload) });
            if (internalOps.length > 256) internalOps.shift();
        },
    };
    return { binding, nodes, children, internalOps };
}
