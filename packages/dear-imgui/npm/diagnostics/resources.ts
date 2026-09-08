import type { NativeCommitOperation, NativeCommitResult } from "@xframes/common";
import type { NativeBinding } from "./bridge";
import { check, waitFor } from "./assertions";
import { observeNativeFrame, readDiagnostics, waitForNativeIdle, type NativeFrame } from "./frames";

export type ResourceFixtureOptions = { baseUrl: string; controlUrl: string; assets: string };
const root = 1901000000;
const create = (id: number, elementType: string, props: Record<string, unknown> = {}): NativeCommitOperation =>
    ({ op: "create", id, elementType, props });
const children = (parentId: number, childrenIds: number[]): NativeCommitOperation => ({ op: "setChildren", parentId, childrenIds });
const resources = (frame: NativeFrame, id: number) => frame.elements.find(element => element.id === id)?.resources;
const pause = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));

/** Actual file/HTTP completion gates on an ordinary initialized backend. Server
 * release happens only after the native loop has proved clean and inactive. */
export async function verifyResources(binding: NativeBinding, options: ResourceFixtureOptions,
    prefetchEvents: () => { id: number; completed: number; total: number }[]) {
    const browser = typeof window !== "undefined";
    const gates: { name: string; observationMs: number; nativeRevision: string; coveredGeneration: string }[] = [];
    const initial = await waitForNativeIdle(binding, "resource fixture starts inactive");
    const baseline = initial.scheduler.ownerCount;
    const asset = (group: string, kind = "image", hold = true, extra = "") =>
        `${options.baseUrl}/asset?group=${group}&kind=${kind}&hold=${hold ? 1 : 0}${extra}`;
    const control = async (action: string, group: string) => {
        const result = await fetch(`${options.controlUrl}/${action}?group=${group}`);
        check(result.ok, `Resource control ${action}/${group} failed: ${result.status}`);
        return action === "state" ? result.json() : result.text();
    };
    const serverStarted = (group: string, count = 1) => waitFor(() => control("state", group),
        state => state.started >= count, `${group}: actual HTTP request started`);
    const apply = (operations: NativeCommitOperation[], rootChildren = [root]) => {
        const state = JSON.parse(binding.getCommitState());
        const result = JSON.parse(binding.applyCommit(JSON.stringify({ schemaVersion: 2, surfaceId: 0,
            baseRevision: state.nativeRevision, rootChildren, operations }))) as NativeCommitResult;
        check(result.status === "applied", `Resource publication failed: ${JSON.stringify(result)}`);
        return result;
    };
    const mount = (definitions: { id: number; type: string; props?: Record<string, unknown> }[]) => apply([
        create(root, "node", { root: true, style: { width: 900, height: 700 } }),
        ...definitions.map(({ id, type, props }) => create(id, type, props)),
        children(root, definitions.map(item => item.id)), ...definitions.map(item => children(item.id, [])),
    ]);
    const op = (id: number, value: Record<string, unknown>) => binding.elementInternalOp(id, JSON.stringify(value));
    const covered = async (name: string, predicate: (frame: NativeFrame) => boolean, started = performance.now()) => {
        const frame = await observeNativeFrame(binding, predicate, name);
        gates.push({ name, observationMs: performance.now() - started, nativeRevision: frame.nativeRevision,
            coveredGeneration: frame.coveredGeneration });
        return frame;
    };
    const assertHeldIdle = async (name: string) => {
        const idle = await waitForNativeIdle(binding, `${name}: pending HTTP permits inactivity`);
        await pause(100); // observe inactivity while the server deliberately holds work
        const after = readDiagnostics(binding);
        check(after.scheduler.constructed === idle.scheduler.constructed && after.scheduler.submitted === idle.scheduler.submitted,
            `${name}: pending resource kept constructing frames`);
        return idle;
    };
    const empty = async () => {
        apply([], []);
        await observeNativeFrame(binding, frame => frame.elementCount === 0, "resource unmount covering frame");
        const idle = await waitForNativeIdle(binding, "resource unmount settles");
        check(idle.scheduler.ownerCount === baseline, "Resource activity owners leaked after acknowledged deletion");
        check(idle.resourceState.textures.liveTextures === 0 && idle.resourceState.textures.retiredTextures === 0,
            "Resource textures remained after the covering empty frame");
        check(idle.resourceState.queuedPrefetchEvents === 0, "Prefetch events survived the covering empty frame");
    };
    try {
        if (browser) {
            // Create and cancel in one JS task, before an IndexedDB lookup or a
            // browser callback could run. No waiting for server receipt here.
            mount([{ id: root + 1, type: "di-image", props: { url: asset("immediate-image"), style: { width: 16, height: 16 } } },
                { id: root + 2, type: "di-js-canvas", props: { style: { width: 16, height: 16 } } }]);
            op(root + 2, { op: "loadTexture", textureId: "immediate", source: asset("immediate-canvas") });
            op(root + 2, { op: "unloadTexture", textureId: "immediate" });
            await empty();
            await control("release", "immediate-image");
            await control("release", "immediate-canvas");
            await covered("same-task Image/Canvas cancellation releases the actual fetch lifetime", frame => frame.elementCount === 0);
        }
        const ids = [root + 1, root + 2, root + 3];
        const imageIds = [...ids, root + 4];
        mount(imageIds.map((id, i) => ({ id, type: "di-image", props: {
            url: browser ? asset("images", i === 1 ? "failure" : i === 3 ? "invalid" : "image", true, `&id=${i}`)
                : i === 1 || i === 3 ? "invalid.png" : "fixture.png",
            style: { width: 80, height: 60, ...(i === 2 ? { display: "none" } : {}) },
        } })));
        if (browser) { await serverStarted("images", 4); await assertHeldIdle("images"); }
        const imageStart = performance.now();
        if (browser) await control("release", "images");
        await covered("multiple Image uploads and failure, including a clipped Image", frame =>
            imageIds.every((id, i) => resources(frame, id)?.loadedTextures === (i === 1 || i === 3 ? 0 : 1)
                && resources(frame, id)?.queuedLoads === 0 && resources(frame, id)?.pendingRequests === 0)
            && resources(frame, ids[1])?.lastLoadFailed === true, imageStart);
        await waitForNativeIdle(binding, "images settle after all jobs drain");
        op(ids[0], { op: "reloadImage" });
        await covered("Image reload replaces the owned texture", frame => resources(frame, ids[0])?.loadedTextures === 1
            && resources(frame, ids[0])?.pendingRequests === 0 && resources(frame, ids[0])?.queuedLoads === 0);
        await empty();

        mount(ids.map((id, i) => ({ id, type: ["di-js-canvas", "di-lua-canvas", "di-janet-canvas"][i],
            props: { style: { width: 180, height: 80 } } })));
        for (const [i, id] of ids.entries()) {
            op(id, { op: "setContinuous", continuous: false });
            op(id, { op: "setScriptFile", path: browser ? asset("canvases", "script", true, `&id=${i}`) : `${options.assets}/script.txt` });
            op(id, { op: "loadTexture", textureId: "fixture", source: browser ? asset("canvases", "image", true, `&id=${i}`) : `${options.assets}/fixture.png` });
        }
        if (browser) { await serverStarted("canvases", 6); await assertHeldIdle("canvases"); }
        const canvasStart = performance.now();
        if (browser) await control("release", "canvases");
        await covered("all three Canvas script and texture completion paths", frame => ids.every(id =>
            resources(frame, id)?.loadedTextures === 1 && resources(frame, id)?.scriptReady === true
            && resources(frame, id)?.queuedLoads === 0 && resources(frame, id)?.pendingRequests === 0), canvasStart);
        await waitForNativeIdle(binding, "static loaded Canvas engines settle");
        for (const id of ids) op(id, { op: "reloadTexture", textureId: "fixture", source: browser
            ? asset("canvases", "image", false) : `${options.assets}/fixture.png` });
        await covered("Canvas reload retains exactly one texture per engine", frame => ids.every(id =>
            resources(frame, id)?.loadedTextures === 1 && resources(frame, id)?.queuedLoads === 0 && resources(frame, id)?.pendingRequests === 0));
        for (const id of ids) op(id, { op: "unloadTexture", textureId: "fixture" });
        await covered("Canvas unload releases all engine textures", frame => ids.every(id => resources(frame, id)?.loadedTextures === 0));
        for (const id of ids) op(id, { op: "loadTexture", textureId: "invalid", source: browser
            ? asset("canvas-invalid", "invalid", false) : `${options.assets}/invalid.png` });
        await covered("Canvas decode failures drain across all engines", frame => ids.every(id => resources(frame, id)?.loadedTextures === 0
            && resources(frame, id)?.lastLoadFailed === true && resources(frame, id)?.queuedLoads === 0 && resources(frame, id)?.pendingRequests === 0));
        for (const id of ids) {
            op(id, { op: "clear" });
            op(id, { op: "setScriptFile", path: browser ? asset("script-failure", "failure", false, `&id=${id}`)
                : `${options.assets}/missing-script.txt` });
        }
        await covered("Canvas script-file failures settle across all engines", frame => ids.every(id =>
            resources(frame, id)?.scriptReady === false && resources(frame, id)?.lastLoadFailed === true
            && resources(frame, id)?.queuedLoads === 0 && resources(frame, id)?.pendingRequests === 0));
        await empty();

        // Image/Canvas browser cancellation closes actual in-flight fetches. On
        // desktop these APIs load files; their queued cancellation is tested by
        // native tests, while Map below supplies controlled delayed HTTP work.
        if (browser) {
            mount([{ id: ids[0], type: "di-image", props: { url: asset("cancel-image") } },
                { id: ids[1], type: "di-js-canvas", props: {} }]);
            op(ids[1], { op: "setContinuous", continuous: false });
            op(ids[1], { op: "loadTexture", textureId: "cancel", source: asset("cancel-canvas") });
            op(ids[1], { op: "setScriptFile", path: asset("cancel-script", "script") });
            await serverStarted("cancel-image"); await serverStarted("cancel-canvas"); await serverStarted("cancel-script");
            await assertHeldIdle("cancelled browser resources");
            op(ids[1], { op: "unloadTexture", textureId: "cancel" });
            op(ids[1], { op: "clear" });
            await empty();
            mount([{ id: ids[0], type: "di-image", props: { url: asset("replacement-image", "image", false) } }]);
            await covered("replacement Image loaded before cancelled requests release", frame => resources(frame, ids[0])?.loadedTextures === 1);
            const idle = await waitForNativeIdle(binding, "replacement Image settles");
            for (const group of ["cancel-image", "cancel-canvas", "cancel-script"]) await control("release", group);
            await pause(100);
            check(readDiagnostics(binding).scheduler.generation === idle.scheduler.generation, "Cancelled browser completion invalidated a replacement lifetime");
            await empty();
        }

        const map = ids[0];
        const mapProps = (group: string, kind = "image") => ({ tileUrlTemplate: asset(group, kind, true, "&z={z}&x={x}&y={y}"),
            attribution: "Local deterministic fixture", style: { width: 256, height: 256 } });
        mount([{ id: map, type: "map-view", props: mapProps("tiles") }]);
        op(map, { op: "render", centerX: 0, centerY: 0, zoom: 1 });
        await serverStarted("tiles", 4); await assertHeldIdle("Map tile downloads");
        const mapStart = performance.now();
        await control("release", "tiles");
        await covered("Map tile completion wakes and uploads every queued tile", frame => resources(frame, map)?.loadedTextures === 4
            && resources(frame, map)?.pendingRequests === 0 && resources(frame, map)?.queuedLoads === 0, mapStart);
        await waitForNativeIdle(binding, "Map tile completion settles");
        await empty();

        mount([{ id: map, type: "map-view", props: mapProps("failed-tiles", "failure") }]);
        op(map, { op: "render", centerX: 0, centerY: 0, zoom: 1 });
        await serverStarted("failed-tiles", 4); await assertHeldIdle("failed Map requests");
        await control("release", "failed-tiles");
        await covered("Map failures finish without a retry render loop", frame => resources(frame, map)?.pendingRequests === 0
            && resources(frame, map)?.lastLoadFailed === true && resources(frame, map)?.failedTiles === 4);
        await assertHeldIdle("failed Map settles");
        check((await control("state", "failed-tiles")).started === 4, "Failed Map tiles retried without a new view request");
        await empty();

        mount([{ id: map, type: "map-view", props: mapProps("cancel-tiles") }]);
        op(map, { op: "render", centerX: 0, centerY: 0, zoom: 1 });
        await serverStarted("cancel-tiles", 4); await assertHeldIdle("Map before removal");
        await empty();
        mount([{ id: map, type: "map-view", props: mapProps("unused-replacement") }]);
        await covered("Map replacement starts without old requests", frame => resources(frame, map)?.pendingRequests === 0);
        const replacementIdle = await waitForNativeIdle(binding, "replacement Map settles");
        await control("release", "cancel-tiles");
        await waitFor(() => readDiagnostics(binding), frame => !frame.resourceState.mapWorkers
            || frame.resourceState.mapWorkers.active === 0 && frame.resourceState.mapWorkers.queued === 0, "cancelled desktop calls finish without a widget reference");
        await pause(100);
        check(readDiagnostics(binding).scheduler.generation === replacementIdle.scheduler.generation, "Removed Map completion reached a reused native ID");
        await empty();

        const eventBaseline = prefetchEvents().length;
        mount([{ id: map, type: "map-view", props: { ...mapProps("prefetch"),
            cachePath: browser ? "/diagnostics-prefetch" : `${options.assets}/prefetch-cache` } }]);
        op(map, { op: "prefetch", minLon: 1, maxLon: 2, minLat: 1, maxLat: 2, minZoom: 1, maxZoom: 1 });
        await serverStarted("prefetch"); await assertHeldIdle("Map prefetch");
        const prefetchStart = performance.now();
        await control("release", "prefetch");
        await covered("Map prefetch progress completion wakes rendering", frame => resources(frame, map)?.prefetchCompleted === 1
            && resources(frame, map)?.prefetchTotal === 1 && resources(frame, map)?.pendingRequests === 0, prefetchStart);
        await waitFor(prefetchEvents, events => events.slice(eventBaseline).some(event => event.id === map && event.completed === 1 && event.total === 1),
            "prefetch completion reaches the application callback");
        await empty();
        return { status: "passed", gates, backend: browser ? "browser HTTP fetch" : "desktop files and bounded Map HTTP workers",
            sources: "local deterministic PNG/script/failure responses; explicit release after native inactivity",
            finalScheduler: readDiagnostics(binding).scheduler, finalResources: readDiagnostics(binding).resourceState };
    } finally {
        const state = JSON.parse(binding.getCommitState());
        if (state.surfaceStatus === "healthy" && state.managedCount > 0) await empty();
    }
}
