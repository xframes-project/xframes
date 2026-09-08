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
const nativeMethods = ["setElement", "patchElement", "setChildren", "appendChild", "elementInternalOp"];

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
    const remove = (id: number) => {
        for (const child of children.get(id) ?? []) remove(child);
        nodes.delete(id);
        children.delete(id);
    };
    const binding: NativeBinding = {
        setElement: (payload: string) => { const node = JSON.parse(payload); nodes.set(node.id, node); children.set(node.id, []); },
        patchElement: (id: number, payload: string) => { if (nodes.has(id)) nodes.set(id, { ...nodes.get(id), ...JSON.parse(payload) }); },
        setChildren: (id: number, payload: string) => {
            const next: number[] = JSON.parse(payload);
            for (const old of children.get(id) ?? []) if (!next.includes(old)) remove(old);
            children.set(id, next);
        },
        appendChild: (parent: number, child: number) => {
            const siblings = children.get(parent);
            if (siblings && !siblings.includes(child)) siblings.push(child);
        },
        elementInternalOp: (id: number, payload: string) => {
            internalOps.push({ id, live: nodes.has(id), op: JSON.parse(payload) });
            if (internalOps.length > 256) internalOps.shift();
        },
    };
    return { binding, nodes, children, internalOps };
}
