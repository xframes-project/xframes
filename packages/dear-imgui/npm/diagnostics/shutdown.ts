import type { NativeBinding } from "./bridge";
import type { ResourceFixtureOptions } from "./resources";
import { check, waitFor } from "./assertions";
import { observeNativeFrame, readDiagnostics, waitForNativeIdle } from "./frames";

/** Terminal cleanup is separate from ordinary acknowledged unmount coverage. */
export async function verifyShutdown(binding: NativeBinding, resources: ResourceFixtureOptions,
    close: () => Promise<void>, terminal: () => any) {
    binding.setDiagnosticsEnabled(true);
    const root = 1903000000, map = root + 1;
    const revision = JSON.parse(binding.getCommitState()).nativeRevision;
    const result = JSON.parse(binding.applyCommit(JSON.stringify({ schemaVersion: 2, surfaceId: 0, baseRevision: revision,
        rootChildren: [root], operations: [
            { op: "create", id: root, elementType: "node", props: { root: true, style: { width: 900, height: 700 } } },
            { op: "create", id: map, elementType: "map-view", props: { style: { width: 256, height: 256 }, attribution: "Local teardown fixture",
                tileUrlTemplate: `${resources.baseUrl}/asset?group=shutdown&hold=1&z={z}&x={x}&y={y}` } },
            { op: "setChildren", parentId: root, childrenIds: [map] }, { op: "setChildren", parentId: map, childrenIds: [] },
        ] })));
    check(result.status === "applied", "Shutdown fixture publication failed");
    binding.elementInternalOp(map, JSON.stringify({ op: "render", centerX: 0, centerY: 0, zoom: 1 }));
    await observeNativeFrame(binding, frame => frame.elements.find(element => element.id === map)?.resources?.pendingRequests === 4,
        "shutdown Map owns four pending requests");
    await waitFor(async () => (await fetch(`${resources.controlUrl}/state?group=shutdown`)).json(), state => state.started === 4,
        "shutdown requests reached the actual local server");
    const idle = await waitForNativeIdle(binding, "window is idle with pending resources before close");
    const beforeCommit = JSON.parse(binding.getCommitState());
    await close();
    await fetch(`${resources.controlUrl}/release?group=shutdown`);
    const stopped = await waitFor(terminal, value => value?.scheduler.status === "disposed" && (!value.resourceState.mapWorkers
        || value.resourceState.mapWorkers.active === 0 && value.resourceState.mapWorkers.queued === 0 && value.resourceState.mapWorkers.threads === 0),
        "disposed runtime releases pending native work");
    check(stopped.commit.surfaceStatus === "disposed" && stopped.commit.initialized === false && stopped.commit.managedCount === 0,
        "Runtime disposal retained managed elements or reported a healthy surface");
    check(stopped.commit.nativeRevision === beforeCommit.nativeRevision && stopped.commit.nativeSequence === beforeCommit.nativeSequence,
        "Runtime disposal invented a structural publication");
    check(stopped.scheduler.ownerCount === 0 && stopped.scheduler.activeOwners === 0 && stopped.scheduler.deadlines === 0
        && !stopped.scheduler.wakeAttached && !stopped.scheduler.notificationPending, "Disposed scheduler retained callbacks/activity");
    check(Object.values(stopped.platform).every(count => count === 0), "Disposed renderer retained platform callbacks/timers");
    check(stopped.scheduler.constructed === idle.scheduler.constructed && stopped.scheduler.submitted === idle.scheduler.submitted,
        "Idle close constructed/submitted an unsolicited final frame");
    check(stopped.resourceState.textures.liveTextures === 0 && stopped.resourceState.textures.retiredTextures === 0
        && stopped.resourceState.queuedPrefetchEvents === 0, "Disposed runtime retained resource ownership");
    return { status: "passed", pendingRequestsAtClose: 4, terminal: stopped,
        observation: "Resource cleanup and terminal metadata; not an acknowledged empty publication or a presentation frame" };
}
