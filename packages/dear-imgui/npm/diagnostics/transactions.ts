import type { NativeCommit, NativeCommitOperation, NativeCommitResult, NativeCommitState } from "@xframes/common";
import { check, waitFor } from "./assertions";
import { observeBinding, type NativeBinding } from "./bridge";
import type { NativeFrame } from "./runtime";

const root = 1900000001, plot = 1900000002, table = 1900000003, second = 1900000004, probe = 1900000005;
const create = (id: number, elementType: string, props: Record<string, unknown> = {}): NativeCommitOperation => ({ op: "create", id, elementType, props });
const children = (parentId: number, childrenIds: number[]): NativeCommitOperation => ({ op: "setChildren", parentId, childrenIds });
const patch = (id: number, props: Record<string, unknown>): NativeCommitOperation => ({ op: "patch", id, props });
type Relationships = [number, number[]][];

/** Same final-tree wire fixtures run against both current-source native modules. */
export async function verifyTransactions(native: NativeBinding, moveCycles = 1) {
    check(typeof native.applyCommit === "function" && typeof native.getCommitState === "function", "Missing native publication exports");
    for (const name of ["setElement", "patchElement", "setChildren", "appendChild"])
        check(native[name] === undefined, `Removed alpha structural export is still present: ${name}`);
    const observer = observeBinding(native);
    const binding = observer.binding;
    const state = (): NativeCommitState => JSON.parse(binding.getCommitState());
    const initial = state();
    check(initial.schemaVersion === 2 && initial.surfaceStatus === "healthy" && initial.managedCount === 0,
        "Publication fixtures require an empty healthy schema-v2 surface");
    let accepted = 0n;
    let roots = [root];
    let relationships: Relationships = [[root, [plot, table]], [plot, []], [table, []]];
    const batch = (operations: NativeCommitOperation[] = [], rootChildren = roots, tree = relationships): NativeCommit => ({
        schemaVersion: 2, surfaceId: 0, baseRevision: String(BigInt(initial.nativeRevision) + accepted), rootChildren,
        operations: [...operations, ...tree.map(([id, ids]) => children(id, ids))],
    });
    const semanticResults: unknown[] = [], timings: unknown[] = [];
    let frame = JSON.parse(binding.getDiagnostics()) as NativeFrame;
    const observe = async (predicate: (frame: NativeFrame) => boolean, label: string) => {
        const previous = frame.frame;
        frame = await waitFor(() => JSON.parse(binding.getDiagnostics()) as NativeFrame,
            next => next.enabled && next.frame > previous && predicate(next), `publication: ${label}`);
        return frame;
    };
    const project = (value: NativeFrame) => ({ elementCount: value.elementCount, hierarchyCount: value.hierarchyCount,
        subjects: value.internalSubjectCount, unreachableCount: value.unreachableCount,
        elements: value.elements.map(({ id, type, children, yogaChildren, yogaParent, state }) =>
            ({ id, type, children, yogaChildren, yogaParent, state })) });
    const apply = (name: string, wire: string, error?: string, index: number | null = null, destroyed: number[] = []) => {
        const start = performance.now();
        const result = JSON.parse(binding.applyCommit(wire)) as NativeCommitResult;
        const boundaryMs = performance.now() - start;
        if (!error) accepted++;
        check(result.status === (error ? "rejected" : "applied"), `${name}: ${JSON.stringify(result)}`);
        check(result.schemaVersion === 2 && result.surfaceId === 0, `${name}: result envelope`);
        check(result.nativeRevision === String(BigInt(initial.nativeRevision) + accepted), `${name}: one revision per accepted publication`);
        check(result.nativeSequence === (error ? null : String(BigInt(initial.nativeSequence) + accepted)), `${name}: authoritative sequence`);
        check(JSON.stringify(result.destroyedIds) === JSON.stringify(destroyed), `${name}: actual destruction ${JSON.stringify(result)}`);
        if (error) check(result.error?.code === error && result.error.operationIndex === index, `${name}: rejection classification ${JSON.stringify(result)}`);
        const current = state();
        check(current.nativeSequence === String(BigInt(initial.nativeSequence) + accepted)
            && current.nativeRevision === result.nativeRevision, `${name}: diagnostics-independent ordering`);
        semanticResults.push({ name, status: result.status, sequenceDelta: error ? null : String(accepted),
            revisionDelta: String(accepted), surfaceId: result.surfaceId, destroyedIds: result.destroyedIds,
            error: result.error ? { code: result.error.code, operationIndex: result.error.operationIndex } : null });
        timings.push({ name, boundaryMs, native: current.lastTransaction });
        return result;
    };
    binding.setDiagnosticsEnabled(true);
    await observe(next => next.elementCount === 0, "initial empty state");
    apply("create-forward-reference-patch", JSON.stringify(batch([
        patch(plot, { showLegend: true }),
        create(root, "node", { root: true, style: { width: 800, height: 600 } }),
        create(plot, "plot-bar", { dataPointsLimit: 128, series: [{ label: "A" }, { label: "B" }], style: { width: 500, height: 250 } }),
        create(table, "di-table", { columns: [{ fieldId: "v", heading: "Value", type: "number" }], style: { width: 500, height: 200 } }),
    ])));
    binding.elementInternalOp(plot, JSON.stringify({ op: "appendSeriesData", seriesIndex: 1, x: 42, y: 17 }));
    binding.elementInternalOp(table, JSON.stringify({ op: "setData", data: [{ v: 19 }] }));
    const populated = (next: NativeFrame) => next.elements.find(node => node.id === plot)?.state.series[1].lastX === 42
        && next.elements.find(node => node.id === table)?.state.rowCount === 1;
    await observe(next => next.elementCount === 3 && populated(next), "populated publication");
    const mounted = project(frame);
    check(mounted.subjects === 2 && mounted.unreachableCount === 0, "Mounted subject/reachability counts");
    const invalidOperations: [string, any, string][] = [
        ["unsupported-op", { op: "future" }, "unsupported_operation"],
        ["removed-append-op", { op: "appendChild", parentId: root, childId: plot }, "unsupported_operation"],
        ["duplicate-create", create(plot, "node"), "duplicate_id"],
        ["missing-patch", patch(second, {}), "missing_target"],
        ["invalid-type", create(second, "unknown"), "invalid_element_type"],
        ["duplicate-child", children(root, [plot, plot, table, probe]), "duplicate_child"],
        ["missing-child", children(root, [plot, second, table, probe]), "missing_target"],
        ["self-link", children(root, [root]), "cycle"],
        ["measured-parent", children(table, [plot]), "invalid_relationship"],
        ["overwrite-id", patch(plot, { id: "public" }), "immutable_identity"],
        ["overwrite-type", patch(plot, { type: "node" }), "immutable_identity"],
        ["nested-series", patch(plot, { series: [{ label: 123 }] }), "invalid_props"],
        ["nested-border", patch(plot, { style: { border: { thickness: "bad" } } }), "invalid_props"],
        ["nested-columns", create(second, "di-table", { columns: [{ heading: "missing fieldId" }] }), "invalid_props"],
        ["bool-shape", patch(plot, { axisAutoFit: "bad" }), "invalid_props"],
        ["props-shape", { op: "patch", id: plot, props: [] }, "invalid_props"],
        ["unknown-field", { ...patch(plot, {}), future: 1 }, "unknown_field"],
    ];
    for (const id of [0, -1, 1.5, 2147483648, 9007199254740992, "2", null])
        invalidOperations.push([`invalid-id-${String(id)}`, { ...create(second, "node"), id }, "invalid_id"]);
    for (const [name, invalid, code] of invalidOperations) {
        const tree: Relationships = [[root, [plot, table, probe]], [plot, []], [table, []], [probe, []]];
        const wire = batch([create(probe, "plot-bar"), patch(plot, { showLegend: false }), invalid], roots,
            tree.filter(([id]) => invalid.op !== "setChildren" || invalid.parentId !== id));
        apply(name, JSON.stringify(wire), code, 2);
        check(!binding.isElementAlive(probe), `${name}: rejected create prefix leaked`);
        await observe(next => next.elementCount === mounted.elementCount, name);
        check(JSON.stringify(project(frame)) === JSON.stringify(mounted), `${name}: rejected publication changed native state`);
    }
    for (const [name, wire, code] of [
        ["malformed-json", "{", "invalid_json"],
        ["removed-version-1", JSON.stringify({ schemaVersion: 1, surfaceId: 0, operations: [] }), "unsupported_version"],
        ["future-version", JSON.stringify({ ...batch(), schemaVersion: 3 }), "unsupported_version"],
        ["unsupported-surface", JSON.stringify({ ...batch(), surfaceId: 1 }), "unsupported_surface"],
        ["typed-surface", JSON.stringify({ ...batch(), surfaceId: "0" }), "unsupported_surface"],
        ["caller-ordering", JSON.stringify({ ...batch(), sequence: 1 }), "unknown_field"],
        ["missing-operations", JSON.stringify({ ...batch(), operations: undefined }), "missing_field"],
        ["missing-root-list", JSON.stringify({ ...batch(), rootChildren: undefined }), "missing_field"],
        ["stale-revision", JSON.stringify({ ...batch(), baseRevision: initial.nativeRevision }), "stale_revision"],
    ]) apply(name, wire, code);
    for (const revision of [1, null, "", "01", "-1", "1.0", "18446744073709551616"])
        apply(`invalid-revision-${String(revision)}`, JSON.stringify({ ...batch(), baseRevision: revision }), "invalid_field");
    apply("missing-leaf-assignment", JSON.stringify(batch([], roots, [[root, [plot, table]], [plot, []]])), "missing_children", 0);
    apply("duplicate-assignment", JSON.stringify(batch([children(plot, [])])), "duplicate_assignment", 2);
    apply("unreachable-create", JSON.stringify(batch([create(probe, "node")])), "unreachable_operation", 0);
    apply("unreachable-patch", JSON.stringify(batch([patch(plot, {})], [root], [[root, []]])), "unreachable_operation", 0);
    apply("destroy-recreate-same-publication", JSON.stringify(batch([create(plot, "plot-bar")], [root], [[root, []]])), "duplicate_id", 0);
    apply("virtual-root-assignment-op", JSON.stringify(batch([children(0, [root])])), "invalid_id", 0);
    apply("duplicate-virtual-root", JSON.stringify(batch([], [root, root])), "duplicate_child");
    await observe(next => next.elementCount === 3, "complete rejected candidate unchanged");
    check(JSON.stringify(project(frame)) === JSON.stringify(mounted), "Rejected complete tree changed committed data or Yoga");
    relationships = [[root, [table, plot]], [plot, []], [table, []]];
    apply("reorder", JSON.stringify(batch()));
    await observe(next => JSON.stringify(next.elements.find(node => node.id === root)?.yogaChildren) === JSON.stringify([table, plot]), "reorder Yoga");
    check(populated(frame), "Reorder lost widget data");
    roots = [root, second]; relationships.push([second, []]);
    apply("second-root", JSON.stringify(batch([create(second, "node", { root: true })])));
    for (let cycle = 0; cycle < moveCycles; cycle++) {
        const plotParent = cycle % 2 ? root : second, tableParent = cycle % 2 ? second : root;
        const tree: Relationships = [[root, cycle % 2 ? [plot] : [table]], [second, cycle % 2 ? [table] : [plot]], [plot, []], [table, []]];
        const result: NativeCommitResult = JSON.parse(binding.applyCommit(JSON.stringify(batch([], roots, tree))));
        accepted++;
        check(result.status === "applied" && result.destroyedIds.length === 0
            && result.nativeRevision === String(BigInt(initial.nativeRevision) + accepted), "Repeated move revision/destruction accounting");
        await observe(next => next.elements.find(node => node.id === plot)?.yogaParent === plotParent
            && next.elements.find(node => node.id === table)?.yogaParent === tableParent, `repeated same-ID move ${cycle}`);
        check(populated(frame) && frame.elementCount === 4 && frame.hierarchyCount === 5
            && frame.internalSubjectCount === 2 && frame.unreachableCount === 0, "Repeated same-ID move lost widget state or ownership");
    }
    semanticResults.push({ name: "repeated-same-ID-moves", cycles: moveCycles, revisionDelta: String(accepted) });
    relationships = [[root, [table]], [second, [plot]], [plot, []], [table, []]];
    apply("same-ID-move", JSON.stringify(batch()));
    await observe(next => next.elements.find(node => node.id === plot)?.yogaParent === second, "same-ID move Yoga");
    check(populated(frame) && frame.internalSubjectCount === 2, "XF-LIFE-005: same-ID move lost widget state or subjects");
    roots = [second]; relationships = [[second, [plot, table]], [plot, []], [table, []]];
    apply("surviving-descendants-escape-removed-root", JSON.stringify(batch()), undefined, null, [root]);
    await observe(next => next.elementCount === 3 && next.elements.find(node => node.id === table)?.yogaParent === second, "partial root removal");
    check(populated(frame), "Escaping removed ancestor lost widget data");
    apply("accepted-no-op", JSON.stringify(batch()));
    for (let i = 0; i < 2; i++) {
        const correlationId = "duplicates-and-gaps-have-no-ordering-meaning";
        check(apply(`correlation-${i}`, JSON.stringify({ ...batch(), correlationId })).correlationId === correlationId, "Correlation echo");
    }
    const beforeNull = project(frame).elements.map(node => ({ id: node.id, state: node.state }));
    apply("guarded-null-props", JSON.stringify(batch([
        patch(plot, { series: null, bullColor: null }), patch(table, { columns: null, contextMenuItems: null, clipRows: null }),
    ])));
    await observe(next => next.elementCount === 3, "guarded null removal");
    check(JSON.stringify(project(frame).elements.map(node => ({ id: node.id, state: node.state }))) === JSON.stringify(beforeNull), "Guarded null removal lost widget data");
    // Direct-publication overhead only; actual React collector costs are separate.
    // The removed per-operation mutation engine is not a benchmark mode.
    const stats = (values: number[]) => {
        const sorted = values.filter(Number.isFinite).sort((a, b) => a - b);
        const at = (p: number) => sorted.length ? sorted[Math.max(0, Math.ceil(sorted.length * p) - 1)] : null;
        return { samples: sorted.length, p50: at(.5), p95: at(.95), p99: at(.99), maximum: at(1) };
    };
    const overhead: unknown[] = [];
    for (let repetition = 0; repetition < 3; repetition++) {
        const samples = { serialization: [] as number[], boundary: [] as number[], parse: [] as number[], validation: [] as number[],
            application: [] as number[], lockWait: [] as number[], lockHeld: [] as number[] };
        let bytes = 0;
        for (let i = 0; i < 100; i++) {
            const serialStart = performance.now();
            const wire = JSON.stringify(batch(Array.from({ length: 4 }, () => patch(plot, { showLegend: true }))));
            samples.serialization.push(performance.now() - serialStart);
            bytes += new TextEncoder().encode(wire).length;
            const start = performance.now();
            const result: NativeCommitResult = JSON.parse(binding.applyCommit(wire));
            samples.boundary.push(performance.now() - start);
            accepted++;
            check(result.status === "applied" && result.destroyedIds.length === 0 && result.nativeRevision === String(BigInt(initial.nativeRevision) + accepted), "Publication overhead accounting");
            const native = state().lastTransaction;
            check(native?.status === "applied", "Missing native publication timing sample");
            samples.parse.push(native.parseMs!); samples.validation.push(native.validationMs!); samples.application.push(native.applicationMs!);
            samples.lockWait.push(native.visibilityLockWaitMs!); samples.lockHeld.push(native.visibilityLockHeldMs!);
        }
        overhead.push({ repetition, mode: "final-tree-publication", patches: 400, childAssignments: 300, boundaryCalls: 100, bytes,
            ...Object.fromEntries(Object.entries(samples).map(([name, values]) => [`${name}Ms`, stats(values)])),
            timingScope: "Validation includes final reachability. Boundary includes native work and result decoding; state queries excluded." });
    }
    semanticResults.push({ name: "three-repetition-overhead-accounting", revisionDelta: String(accepted) });
    binding.setDiagnosticsEnabled(false);
    relationships = [[second, [plot]], [plot, []]];
    apply("diagnostics-disabled", JSON.stringify(batch([patch(plot, { showLegend: false })])), undefined, null, [table]);
    check(binding.isElementAlive(plot) && !binding.isElementAlive(table), "Synchronous diagnostics-disabled publication");
    binding.setDiagnosticsEnabled(true);
    await observe(next => next.elementCount === 2 && next.internalSubjectCount === 1, "diagnostics-disabled state");
    roots = []; relationships = [];
    apply("populated-unmount", JSON.stringify(batch()), undefined, null, [plot, second]);
    apply("destroyed-lifetime-cannot-move-later", JSON.stringify(batch([], [plot], [[plot, []]])), "missing_target", 0);
    apply("repeated-empty-unmount", JSON.stringify(batch()));
    roots = [root]; relationships = [[root, [probe, probe + 1, probe + 2]], [probe, []], [probe + 1, []], [probe + 2, []]];
    apply("canvas-bootstrap-create", JSON.stringify(batch([
        create(root, "node", { root: true }), create(probe, "di-js-canvas"),
        create(probe + 1, "di-lua-canvas"), create(probe + 2, "di-janet-canvas"),
    ])));
    for (const id of [probe, probe + 1, probe + 2]) binding.elementInternalOp(id, JSON.stringify({ op: "setData", data: { value: 42 } }));
    await observe(next => next.elementCount === 4 && next.internalSubjectCount === 3
        && next.elements.filter(node => [probe, probe + 1, probe + 2].includes(node.id)).every(node => node.lastInternalOpMs !== null),
        "all Canvas engines initialize and accept data");
    roots = []; relationships = [];
    apply("canvas-bootstrap-cleanup", JSON.stringify(batch()), undefined, null, [probe, probe + 1, probe + 2, root]);
    await observe(next => next.elementCount === 0 && next.internalSubjectCount === 0 && next.hierarchyCount === 1, "final cleanup");
    return { status: "passed", schemaVersion: 2, moveCycles, initial, final: state(), semanticResults, timings, overhead, boundary: observer.snapshot(),
        mounted, finalState: project(frame), completion: "Synchronous result, then a newer observed native frame; no presented-frame/revision claim" };
}
