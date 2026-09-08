import React from "react";
import type { NativeBinding, createBridge } from "./bridge";
import type { ResourceFixtureOptions } from "./resources";
import { check, waitFor } from "./assertions";
import { observeNativeFrame, readDiagnostics, waitForNativeIdle } from "./frames";
import type { NativeCommitOperation } from "@xframes/common";

export async function resourceMoveStress(binding: NativeBinding, options: ResourceFixtureOptions, cycles: number) {
    const initial = await waitForNativeIdle(binding, "resource move owner baseline");
    const first = 1904000000, second = first + 1, map = first + 2;
    const canvasIds = [first + 3, first + 4, first + 5];
    const types = ["di-js-canvas", "di-lua-canvas", "di-janet-canvas"];
    const all = [map, ...canvasIds];
    const create = (id: number, elementType: string, props: any): NativeCommitOperation => ({ op: "create", id, elementType, props });
    const children = (parentId: number, childrenIds: number[]): NativeCommitOperation => ({ op: "setChildren", parentId, childrenIds });
    const apply = (operations: NativeCommitOperation[], roots = [first, second]) => {
        const before = JSON.parse(binding.getCommitState());
        const result = JSON.parse(binding.applyCommit(JSON.stringify({ schemaVersion: 2, surfaceId: 0,
            baseRevision: before.nativeRevision, rootChildren: roots, operations })));
        check(result.status === "applied" && BigInt(result.nativeRevision) === BigInt(before.nativeRevision) + 1n,
            `Resource move publication/revision failed: ${JSON.stringify(result)}`);
        return result;
    };
    const source = `${options.baseUrl}/asset?group=resource-moves&hold=1&z={z}&x={x}&y={y}`;
    apply([create(first, "node", { root: true, style: { width: 900, height: 700 } }),
        create(second, "node", { root: true, style: { width: 900, height: 700 } }),
        create(map, "map-view", { style: { width: 256, height: 256 }, attribution: "Local move fixture", tileUrlTemplate: source }),
        ...canvasIds.map((id, index) => create(id, types[index], { style: { width: 80, height: 60 } })),
        children(first, all), children(second, []), ...all.map(id => children(id, []))]);
    binding.elementInternalOp(map, JSON.stringify({ op: "render", centerX: 0, centerY: 0, zoom: 1 }));
    for (const id of canvasIds) {
        binding.elementInternalOp(id, JSON.stringify({ op: "setScript", script: "" }));
        binding.elementInternalOp(id, JSON.stringify({ op: "loadTexture", textureId: "move",
            source: typeof window === "undefined" ? `${options.assets}/fixture.png`
                : `${options.baseUrl}/asset?group=move-textures&hold=0&id=${id}` }));
    }
    const ready = await observeNativeFrame(binding, frame => frame.scheduler.reasons.canvas.activeOwners === 3
        && frame.elements.find(node => node.id === map)?.resources?.pendingRequests === 4
        && canvasIds.every(id => frame.elements.find(node => node.id === id)?.resources?.loadedTextures === 1),
        "move fixture owns pending HTTP, Canvas textures and animation activity");
    await waitFor(async () => (await fetch(`${options.controlUrl}/state?group=resource-moves`)).json(), state => state.started === 4,
        "resource move HTTP requests started");
    const revision = JSON.parse(binding.getCommitState()).nativeRevision;
    let parent = first;
    for (let cycle = 0; cycle < cycles; ++cycle) {
        parent = cycle % 2 === 0 ? second : first;
        const result = apply([children(first, parent === first ? all : []), children(second, parent === second ? all : []),
            ...all.map(id => children(id, []))]);
        check(result.destroyedIds.length === 0 && readDiagnostics(binding).scheduler.ownerCount === ready.scheduler.ownerCount,
            `Resource move ${cycle} replaced a native resource/activity lifetime`);
    }
    const moved = await observeNativeFrame(binding, frame => all.every(id => frame.elements.find(node => node.id === id)?.yogaParent === parent)
        && frame.scheduler.reasons.canvas.activeOwners === 3
        && frame.elements.find(node => node.id === map)?.resources?.pendingRequests === 4
        && canvasIds.every(id => frame.elements.find(node => node.id === id)?.resources?.loadedTextures === 1),
        "submitted frame preserves moved resource and animation ownership");
    check(BigInt(moved.nativeRevision) === BigInt(revision) + BigInt(cycles), "Resource moves changed structural call accounting");
    for (const id of canvasIds) binding.elementInternalOp(id, JSON.stringify({ op: "setContinuous", continuous: false }));
    await waitForNativeIdle(binding, "moved static resources permit inactivity with pending HTTP");
    await fetch(`${options.controlUrl}/release?group=resource-moves`);
    await observeNativeFrame(binding, frame => frame.elements.find(node => node.id === map)?.resources?.loadedTextures === 4,
        "completion after moves reaches the preserved Map owner");
    apply([], []);
    await observeNativeFrame(binding, frame => frame.elementCount === 0, "resource move cleanup");
    const final = await waitForNativeIdle(binding, "resource move inactivity");
    check(final.scheduler.ownerCount === initial.scheduler.ownerCount && final.resourceState.textures.liveTextures === 0
        && final.resourceState.textures.retiredTextures === 0, "Resource moves retained owners/textures");
    return { status: "passed", cycles, revisionDelta: cycles, activeCanvases: 3, pendingMapRequests: 4,
        completionAfterMove: "passed", finalScheduler: final.scheduler, finalPlatform: final.platform };
}

/** One ordinary Fabric lifetime with active canvases and outstanding HTTP work.
 * Reuses bounded server groups; each cycle drains its late completions. */
export async function resourceStressCycle(binding: NativeBinding, bridge: ReturnType<typeof createBridge>,
    options: ResourceFixtureOptions, cycle: number, ownerBaseline: number) {
    const control = async (action: string) => {
        const response = await fetch(`${options.controlUrl}/${action}?group=stress-resources`);
        check(response.ok, `Resource stress ${action} failed: ${response.status}`);
        return action === "state" ? response.json() : response.text();
    };
    await control("reset");
    const source = `${options.baseUrl}/asset?group=stress-resources&hold=1&cycle=${cycle}`;
    const canvasTypes = ["di-js-canvas", "di-lua-canvas", "di-janet-canvas"];
    await bridge.render(React.createElement("node", { root: true, id: "stress-resource-root", style: { width: 900, height: 700 } },
        React.createElement("map-view", { id: "stress-map", attribution: "Local lifecycle fixture", style: { width: 256, height: 256 },
            tileUrlTemplate: `${source}&z={z}&x={x}&y={y}` }),
        ...canvasTypes.map((type, index) => React.createElement(type, { key: type, id: `stress-canvas-${index}`, style: { width: 80, height: 60 } })),
        React.createElement("di-image", { id: "stress-image", style: { width: 16, height: 16 },
            url: typeof window === "undefined" ? "fixture.png" : `${options.baseUrl}/asset?group=stress-image&hold=0` })));
    const map = bridge.registrations.captureWidget("stress-map")!.nativeId;
    binding.elementInternalOp(map, JSON.stringify({ op: "render", centerX: 0, centerY: 0, zoom: 1 }));
    await observeNativeFrame(binding, frame => frame.elements.find(node => node.id === map)?.resources?.pendingRequests === 4,
        `resource stress ${cycle}: four pending Map requests`);
    await waitFor(() => control("state"), state => state.started >= 4, `resource stress ${cycle}: actual HTTP requests`);
    for (let index = 0; index < canvasTypes.length; ++index) {
        const id = bridge.registrations.captureWidget(`stress-canvas-${index}`)!.nativeId;
        binding.elementInternalOp(id, JSON.stringify({ op: "setScript", script: "" }));
        binding.elementInternalOp(id, JSON.stringify({ op: "loadTexture", textureId: "pending",
            source: typeof window === "undefined" ? `${options.assets}/fixture.png` : `${source}&canvas=${index}` }));
    }
    const active = await observeNativeFrame(binding, frame => frame.scheduler.reasons.canvas.activeOwners === 3
        && frame.elements.filter(node => canvasTypes.includes(node.type)).every(node => typeof window === "undefined"
            ? node.resources?.loadedTextures === 1 : node.resources?.pendingRequests === 1),
        `resource stress ${cycle}: all Canvas engines active before deletion`);
    await bridge.render(null);
    await observeNativeFrame(binding, frame => frame.elementCount === 0, `resource stress ${cycle}: ordinary empty publication`);
    const removed = await waitForNativeIdle(binding, `resource stress ${cycle}: removed owners settle`);
    await control("release");
    await waitFor(() => readDiagnostics(binding), frame => !frame.resourceState.mapWorkers
        || frame.resourceState.mapWorkers.active === 0 && frame.resourceState.mapWorkers.queued === 0,
        `resource stress ${cycle}: cancelled workers finish`);
    const final = readDiagnostics(binding);
    check(final.scheduler.ownerCount === ownerBaseline && final.scheduler.activeOwners === 0 && final.scheduler.deadlines === 0,
        `Resource stress ${cycle} retained scheduler owners/activity`);
    check(final.resourceState.textures.liveTextures === 0 && final.resourceState.textures.retiredTextures === 0
        && final.resourceState.queuedPrefetchEvents === 0, `Resource stress ${cycle} retained texture/event ownership`);
    check(final.scheduler.reasons.resource.invalidations === removed.scheduler.reasons.resource.invalidations,
        `Resource stress ${cycle}: late completion revived a deleted resource owner`);
    check(final.platform.animationCallbacks === 0 && final.platform.deadlineTimers === 0,
        `Resource stress ${cycle} retained platform scheduling work`);
    return { pendingMapRequests: 4, activeCanvases: active.scheduler.reasons.canvas.activeOwners,
        scheduler: final.scheduler, platform: final.platform, resources: final.resourceState };
}
