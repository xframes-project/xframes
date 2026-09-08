import React from "react";
import { createRoot } from "react-dom/client";
import { createReactNativeHost } from "@xframes/common";
import { ReactNativeWrapper } from "../wasm/src/lib/ReactNativeWrapper";
import { Fixture, makeHandles, makeRows } from "./fixture";
import { check, waitFor } from "./assertions";
import type { NativeFrame } from "./runtime";

/** Exercise the published wrapper's update and teardown path in a real DOM root. */
export async function verifyWasmWrapper(native: any) {
    const host = createReactNativeHost();
    const element = document.createElement("div");
    document.body.append(element);
    const root = createRoot(element);
    const handles = makeHandles();
    let unmounted = false;
    let clicks = 0;
    let rootUnmounted = false;
    const read = () => JSON.parse(native.getDiagnostics()) as NativeFrame;
    native.setDiagnosticsEnabled(true);
    const tree = (reversed = false) => <React.StrictMode>
        <ReactNativeWrapper host={host} wasmModule={native} onUnmount={() => { unmounted = true; }}>
            <Fixture handles={handles} reversed={reversed} onStationClick={() => { clicks++; }} />
        </ReactNativeWrapper>
    </React.StrictMode>;
    try {
        root.render(tree());
        await waitFor(() => handles.table.current && handles.plot.current, Boolean, "wrapper imperative refs");
        handles.table.current!.setTableData(makeRows(2));
        const mounted = await waitFor(read, frame => frame.elementCount === 8 && frame.elements.some(node =>
            node.type === "di-table" && node.state.rowCount === 2), "Strict Mode wrapper populated state");
        const tableId = mounted.elements.find(node => node.type === "di-table")!.id;
        const station = mounted.elements.find(node => node.type === "di-button")!.id;
        host.nativeFabricUIManager.enqueueEvent(station, "onClick", {});
        await waitFor(() => clicks, count => count === 1, "wrapper deferred event");
        root.render(tree(true));
        await waitFor(read, frame => frame.frame > mounted.frame && frame.elements.some(node =>
            node.id === tableId && node.state.rowCount === 2), "wrapper update preserves table identity and state");
        check(!unmounted, "Strict Mode cleanup shut down a live wrapper");
        const saved = handles.table.current!;
        root.unmount();
        rootUnmounted = true;
        await waitFor(() => unmounted, Boolean, "wrapper unmount completion");
        check(host.nativeFabricUIManager.getDiagnostics().subscriptionClosed, "Wrapper did not dispose its bridge");
        saved.setTableData(makeRows(1));
        host.nativeFabricUIManager.enqueueEvent(station, "onClick", {});
        const empty = await waitFor(read, frame => frame.frame > mounted.frame && frame.elementCount === 0,
            "wrapper populated root destruction");
        check(empty.hierarchyCount === 1 && empty.internalSubjectCount === 0 && clicks === 1,
            "Wrapper retained native state or delivered a stale event");
        return { status: "passed", mountedElements: mounted.elementCount, finalElements: empty.elementCount,
            finalHierarchy: empty.hierarchyCount, finalSubjects: empty.internalSubjectCount };
    } finally {
        if (!rootUnmounted) root.unmount();
        element.remove();
        native.setDiagnosticsEnabled(false);
    }
}
