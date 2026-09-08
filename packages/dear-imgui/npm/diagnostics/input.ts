import type { NativeCommitOperation } from "@xframes/common";
import type { NativeBinding } from "./bridge";
import { check, waitFor } from "./assertions";
import { observeNativeFrame, readDiagnostics, waitForNativeIdle } from "./frames";
import type { ResourceFixtureOptions } from "./resources";

export type FixtureInput = { action: "move" | "click" | "text" | "keyDown" | "keyUp" | "wheel" | "close"
    | "minimize" | "restore" | "refresh"; x?: number; y?: number; value?: string };
export type FixtureEvent = { kind: "text" | "number"; id: number; value: string | number };
const root = 1902000000, editor = root + 1, map = root + 2;

export async function verifyInput(binding: NativeBinding, input: (request: FixtureInput) => Promise<void>,
    events: () => FixtureEvent[], resources: ResourceFixtureOptions) {
    const apply = (operations: NativeCommitOperation[], roots = [root]) => {
        const state = JSON.parse(binding.getCommitState());
        const result = JSON.parse(binding.applyCommit(JSON.stringify({ schemaVersion: 2, surfaceId: 0, baseRevision: state.nativeRevision,
            rootChildren: roots, operations })));
        check(result.status === "applied", `Input fixture publication failed: ${JSON.stringify(result)}`);
    };
    const mount = (id: number, elementType: string, props: Record<string, unknown>) => apply([
        { op: "create", id: root, elementType: "node", props: { root: true, style: { width: 900, height: 700 } } },
        { op: "create", id, elementType, props }, { op: "setChildren", parentId: root, childrenIds: [id] },
        { op: "setChildren", parentId: id, childrenIds: [] },
    ]);
    const empty = async () => {
        apply([], []);
        await observeNativeFrame(binding, frame => frame.elementCount === 0, "input fixture empty covering frame");
        return waitForNativeIdle(binding, "input fixture empty inactivity");
    };
    const original = await waitForNativeIdle(binding, "input fixture starts from inactivity");
    const eventStart = events().length;
    try {
        mount(editor, "input-text", { defaultValue: "", style: { width: 260, height: 40 } });
        const inactive = await waitForNativeIdle(binding, "unfocused editor settles");
        await input({ action: "move", x: 50, y: 10 });
        await waitForNativeIdle(binding, "editor hover settles before click");
        await input({ action: "click", x: 50, y: 10 });
        await waitFor(() => readDiagnostics(binding), frame => frame.scheduler.reasons.cursor.deadlines === 1,
            "native click focuses the editor");
        await input({ action: "text", value: "alpha" });
        await waitFor(events, values => values.slice(eventStart).some(event => event.kind === "text" && event.id === editor && event.value === "alpha"),
            "native text reaches the application event callback");
        await observeNativeFrame(binding, frame => frame.elements.some(element => element.id === editor), "text input covering frame");
        const focused = await waitFor(() => readDiagnostics(binding), frame => frame.scheduler.reasons.cursor.deadlines === 1
            && frame.scheduler.activeOwners === 0, "focused editor uses a cursor deadline without continuous activity");
        check(BigInt(focused.scheduler.reasons.input.invalidations) > BigInt(inactive.scheduler.reasons.input.invalidations), "Native input did not invalidate");
        await waitFor(() => readDiagnostics(binding), frame => BigInt(frame.scheduler.reasons.cursor.deadlinesFired)
            > BigInt(focused.scheduler.reasons.cursor.deadlinesFired), "cursor blink advances through its owned deadline");
        const repeatBefore = readDiagnostics(binding).scheduler.reasons.keyRepeat.deadlinesFired;
        const repeatEventStart = events().length;
        try {
            await input({ action: "keyDown" });
            await waitFor(events, values => values.slice(repeatEventStart).some(event => event.kind === "text" && event.id === editor
                && typeof event.value === "string" && event.value.length <= 2), "held Backspace repeats through ImGui");
            check(BigInt(readDiagnostics(binding).scheduler.reasons.keyRepeat.deadlinesFired) > BigInt(repeatBefore),
                "Held key was rescued by another frame instead of its key-repeat deadline");
        } finally { await input({ action: "keyUp" }); }
        await input({ action: "click", x: 500, y: 500 });
        await waitForNativeIdle(binding, "released key and blurred editor return to inactivity");
        await empty();

        mount(map, "map-view", { style: { width: 256, height: 256 }, attribution: "Local input fixture",
            tileUrlTemplate: `${resources.baseUrl}/asset?group=input-map&hold=0&z={z}&x={x}&y={y}` });
        binding.elementInternalOp(map, JSON.stringify({ op: "render", centerX: 0, centerY: 0, zoom: 1 }));
        await observeNativeFrame(binding, frame => frame.elements.find(element => element.id === map)?.resources?.loadedTextures === 4,
            "input Map initial tiles uploaded");
        await input({ action: "move", x: 64, y: 64 });
        const mapIdle = await waitForNativeIdle(binding, "hovered Map settles before zoom");
        const beforeRequests = await (await fetch(`${resources.controlUrl}/state?group=input-map`)).json();
        await input({ action: "wheel", x: 64, y: 64 });
        await waitFor(events, values => values.slice(eventStart).some(event => event.kind === "number" && event.id === map && event.value === 2),
            "native wheel zoom reaches the Map callback");
        await observeNativeFrame(binding, frame => (frame.elements.find(element => element.id === map)?.resources?.loadedTextures ?? 0) > 4
            && BigInt(frame.scheduler.reasons.map.deadlinesFired) > BigInt(mapIdle.scheduler.reasons.map.deadlinesFired),
            "Map's zoom deadline fetches and uploads the new tiles without rescue input");
        const afterRequests = await (await fetch(`${resources.controlUrl}/state?group=input-map`)).json();
        check(afterRequests.started > beforeRequests.started, "Zoom did not reach the actual tile fetch path");
        await waitForNativeIdle(binding, "finite Map zoom returns to inactivity");
        const final = await empty();
        check(final.scheduler.ownerCount === original.scheduler.ownerCount, "Input/deadline owners leaked");
        return { status: "passed", events: events().slice(eventStart), source: typeof window === "undefined" ? "native window events" : "Chromium CDP input",
            cursorDeadline: "passed", keyRepeatDeadline: "passed", mapZoomDeadline: "passed", finalScheduler: final.scheduler };
    } catch (error) {
        throw new Error(`${error}; input snapshot: ${JSON.stringify(readDiagnostics(binding))}; events: ${JSON.stringify(events().slice(eventStart))}`);
    } finally {
        if (JSON.parse(binding.getCommitState()).managedCount) await empty();
    }
}
