import type { NativeCommit, NativeCommitOperation, NativeCommitResult, NativeCommitState } from "@xframes/common";
import { check, waitFor } from "./assertions";
import { observeBinding, type NativeBinding } from "./bridge";
import type { NativeFrame } from "./runtime";

const root = 1900000001, plot = 1900000002, table = 1900000003, second = 1900000004;
const create = (id: number, elementType: string, props: Record<string, unknown> = {}): NativeCommitOperation => ({ op: "create", id, elementType, props });
const children = (parentId: number, childrenIds: number[]): NativeCommitOperation => ({ op: "setChildren", parentId, childrenIds });
const patch = (id: number, props: Record<string, unknown>): NativeCommitOperation => ({ op: "patch", id, props });
const append = (parentId: number, childId: number): NativeCommitOperation => ({ op: "appendChild", parentId, childId });
const batch = (operations: NativeCommitOperation[]): NativeCommit => ({ schemaVersion: 1, surfaceId: 0, operations });

/** One set of wire fixtures, executed by both current-source native modules. */
export async function verifyTransactions(native: NativeBinding) {
    check(typeof native.applyCommit === "function" && typeof native.getCommitState === "function",
        "Rebuild current native sources: transaction exports are missing");
    const observer = observeBinding(native);
    const binding = observer.binding;
    const state = (): NativeCommitState => JSON.parse(binding.getCommitState());
    const initial = state();
    let accepted = 0n;
    const semanticResults: unknown[] = [];
    const timings: unknown[] = [];
    let frame = JSON.parse(binding.getDiagnostics()) as NativeFrame;
    const observe = async (predicate: (frame: NativeFrame) => boolean, label: string) => {
        const previous = frame.frame;
        frame = await waitFor(() => JSON.parse(binding.getDiagnostics()) as NativeFrame,
            next => next.enabled && next.frame > previous && predicate(next), `transaction: ${label}`);
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
        check(result.schemaVersion === 1 && result.surfaceId === 0, `${name}: result envelope`);
        check(result.nativeRevision === String(BigInt(initial.nativeRevision) + accepted), `${name}: one revision per accepted transaction`);
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
    apply("create-attach-patch", JSON.stringify(batch([
        create(root, "node", { root: true, style: { width: 800, height: 600 } }),
        create(plot, "plot-bar", { dataPointsLimit: 128, series: [{ label: "A" }, { label: "B" }], style: { width: 500, height: 250 } }),
        create(table, "di-table", { columns: [{ fieldId: "v", heading: "Value", type: "number" }], style: { width: 500, height: 200 } }),
        append(root, plot), append(root, table), children(0, [root]),
        patch(plot, { series: [{ label: "Updated" }, { label: "B" }] }),
    ])));
    binding.elementInternalOp(plot, JSON.stringify({ op: "appendSeriesData", seriesIndex: 1, x: 42, y: 17 }));
    binding.elementInternalOp(table, JSON.stringify({ op: "setData", data: [{ v: 19 }] }));
    await observe(next => next.elementCount === 3 && next.elements.find(node => node.id === plot)?.state.series[1].lastX === 42
        && next.elements.find(node => node.id === table)?.state.rowCount === 1, "populated batch");
    const mounted = project(frame);
    check(mounted.subjects === 2 && mounted.unreachableCount === 0, "transaction mounted subject/reachability counts");
    const invalidOperations: [string, unknown, string][] = [
        ["unsupported-op", { op: "future" }, "unsupported_operation"],
        ["duplicate-create", create(plot, "node"), "duplicate_id"],
        ["missing-patch", patch(second, {}), "missing_target"],
        ["invalid-type", create(second, "unknown"), "invalid_element_type"],
        ["duplicate-child", children(root, [plot, plot]), "duplicate_child"],
        ["missing-child", children(root, [plot, second]), "missing_target"],
        ["self-link", children(root, [root]), "cycle"],
        ["forward-reference", append(root, second), "missing_target"],
        ["measured-parent", append(table, plot), "multiple_parents"],
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
        const wire = { ...batch([create(second + 1, "plot-bar"), patch(plot, { showLegend: true })]),
            operations: [create(second + 1, "plot-bar"), patch(plot, { showLegend: true }), invalid] };
        apply(name, JSON.stringify(wire), code, 2);
        check(!binding.isElementAlive(second + 1), `${name}: rejected create prefix leaked`);
        await observe(next => next.elementCount === mounted.elementCount, name);
        check(JSON.stringify(project(frame)) === JSON.stringify(mounted), `${name}: rejected batch changed native state`);
    }
    for (const [name, wire, code] of [
        ["malformed-json", "{", "invalid_json"],
        ["unsupported-version", JSON.stringify({ ...batch([]), schemaVersion: 2 }), "unsupported_version"],
        ["unsupported-surface", JSON.stringify({ ...batch([]), surfaceId: 1 }), "unsupported_surface"],
        ["typed-surface", JSON.stringify({ ...batch([]), surfaceId: "0" }), "unsupported_surface"],
        ["caller-ordering", JSON.stringify({ ...batch([]), sequence: 1 }), "unknown_field"],
        ["missing-operations", '{"schemaVersion":1,"surfaceId":0}', "missing_field"],
    ]) apply(name, wire, code);
    apply("destroyed-reference", JSON.stringify(batch([children(root, []), patch(plot, {})])), "destroyed_id", 1);
    apply("destroy-recreate", JSON.stringify(batch([children(root, []), create(plot, "plot-bar")])), "destroyed_id", 1);
    await observe(next => next.elementCount === 3, "rejected destruction");
    check(JSON.stringify(project(frame)) === JSON.stringify(mounted), "rejected destruction changed native state");
    apply("reorder", JSON.stringify(batch([children(root, [table, plot]), patch(plot, { showLegend: true })])));
    await observe(next => JSON.stringify(next.elements.find(node => node.id === root)?.yogaChildren) === JSON.stringify([table, plot]), "reorder Yoga");
    check(frame.elements.find(node => node.id === plot)?.state.series[1].lastX === 42
        && frame.elements.find(node => node.id === table)?.state.rowCount === 1, "reorder lost widget data");
    apply("second-root", JSON.stringify(batch([create(second, "node", { root: true }), children(0, [root, second])])));
    apply("partial-root-removal", JSON.stringify(batch([children(0, [root])])), undefined, null, [second]);
    apply("empty", JSON.stringify(batch([])));
    apply("idempotent-append", JSON.stringify(batch([append(root, plot)])));
    const correlated = { ...batch([]), correlationId: "duplicates-and-gaps-have-no-ordering-meaning" };
    for (let i = 0; i < 2; i++) {
        const result = apply(`correlation-${i}`, JSON.stringify(correlated));
        check(result.correlationId === correlated.correlationId, "correlation echo");
    }
    // Every compatibility entry must advance exactly the same counters once.
    const legacy = (name: string, fn: () => void) => {
        fn(); accepted++;
        const current = state();
        check(current.nativeRevision === String(BigInt(initial.nativeRevision) + accepted)
            && current.nativeSequence === String(BigInt(initial.nativeSequence) + accepted), `${name}: compatibility ordering`);
        semanticResults.push({ name, revisionDelta: String(accepted) });
    };
    legacy("legacy-create", () => binding.setElement(JSON.stringify({ id: second, type: "node" })));
    legacy("legacy-patch", () => binding.patchElement(second, "{}"));
    legacy("legacy-append", () => binding.appendChild(root, second));
    legacy("legacy-children", () => check(binding.setChildren(root, JSON.stringify([plot, table])) === JSON.stringify([second]), "legacy destruction wire shape"));
    legacy("stale-patch", () => binding.patchElement(second, "{}"));
    legacy("stale-children", () => check(binding.setChildren(second, "[]") === "[]", "stale children no-op"));
    legacy("stale-append", () => binding.appendChild(second, plot));
    const beforeNullRemoval = project(frame).elements.map(node => ({ id: node.id, state: node.state }));
    legacy("legacy-null-series", () => binding.patchElement(plot, JSON.stringify({ series: null, bullColor: null })));
    legacy("legacy-null-columns", () => binding.patchElement(table, JSON.stringify({ columns: null, contextMenuItems: null, clipRows: null })));
    await observe(next => next.elementCount === 3, "guarded null prop removal");
    check(JSON.stringify(project(frame).elements.map(node => ({ id: node.id, state: node.state }))) === JSON.stringify(beforeNullRemoval),
        "guarded null prop removal lost widget data");
    // Bounded microbenchmarks complement the unchanged streaming workload. The
    // legacy case still crosses once per operation; this is not Fabric batching.
    const overhead: unknown[] = [];
    const stats = (values: number[]) => {
        const sorted = values.filter(Number.isFinite).sort((a, b) => a - b);
        const at = (p: number) => sorted.length ? sorted[Math.max(0, Math.ceil(sorted.length * p) - 1)] : null;
        return { samples: sorted.length, p50: at(.5), p95: at(.95), p99: at(.99), maximum: at(1) };
    };
    for (let repetition = 0; repetition < 3; repetition++) {
        for (const mode of ["batch", "compatibility"] as const) {
            const serialization: number[] = [], boundary: number[] = [], parse: number[] = [], envelope: number[] = [],
                validation: number[] = [], application: number[] = [];
            let bytes = 0;
            const calls = mode === "batch" ? 100 : 400;
            for (let i = 0; i < calls; i++) {
                const serializeStart = performance.now();
                const wire = JSON.stringify(mode === "batch" ? batch(Array.from({ length: 4 }, () => patch(plot, { showLegend: true }))) : { showLegend: true });
                serialization.push(performance.now() - serializeStart);
                bytes += new TextEncoder().encode(wire).length;
                const start = performance.now();
                if (mode === "batch") {
                    const result: NativeCommitResult = JSON.parse(binding.applyCommit(wire));
                    check(result.status === "applied" && result.destroyedIds.length === 0, "transaction overhead batch failed");
                } else binding.patchElement(plot, wire);
                boundary.push(performance.now() - start);
                accepted++;
                const current = state();
                check(current.nativeRevision === String(BigInt(initial.nativeRevision) + accepted), "overhead native revision accounting");
                const native = current.lastTransaction;
                check(native?.status === "applied", "Missing native transaction timing sample");
                if (native.parseMs !== undefined) parse.push(native.parseMs);
                if (native.envelopeMs !== undefined) envelope.push(native.envelopeMs);
                validation.push(native.validationMs!); application.push(native.applicationMs!);
            }
            overhead.push({ repetition, mode, operations: 400, boundaryCalls: calls, bytes,
                serializationMs: stats(serialization), boundaryMs: stats(boundary), parseMs: stats(parse),
                envelopeMs: stats(envelope), validationMs: stats(validation), applicationMs: stats(application),
                timingScope: "Batch parse includes JSON/envelope decoding. Compatibility envelope excludes legacy JSON decoding before dispatch. Boundary includes native work and result decoding. Queries excluded." });
        }
    }
    semanticResults.push({ name: "three-repetition-overhead-accounting", revisionDelta: String(accepted) });
    binding.setDiagnosticsEnabled(false);
    apply("diagnostics-disabled", JSON.stringify(batch([patch(plot, { showLegend: false }), children(root, [plot])])), undefined, null, [table]);
    check(binding.isElementAlive(plot) && !binding.isElementAlive(table), "synchronous diagnostics-disabled application");
    binding.setDiagnosticsEnabled(true);
    await observe(next => next.elementCount === 2 && next.internalSubjectCount === 1, "diagnostics-disabled state");
    apply("populated-unmount", JSON.stringify(batch([children(0, [])])), undefined, null, [plot, root]);
    apply("repeated-empty-unmount", JSON.stringify(batch([children(0, [])])));
    await observe(next => next.elementCount === 0 && next.internalSubjectCount === 0 && next.hierarchyCount === 1, "final cleanup");
    return { status: "passed", initial, final: state(), semanticResults, timings, overhead, boundary: observer.snapshot(),
        mounted, finalState: project(frame), completion: "Synchronous result, then a newer observed native frame; no presented-frame/revision claim" };
}
