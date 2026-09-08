import { Subject, Subscription } from "rxjs";
import { WidgetRegistrationService } from "../widgetRegistrationService";
import type { NativeCommitOperation, NativeCommitResult, NativeCommitState } from "../nativeCommit";
import { captureEventProps, readEventProps, type EventProps } from "./fabricEventProps";

type Props = Readonly<Record<string, any>>;
type HostNode = {
    readonly owner: object;
    readonly surfaceId: 0;
    readonly id: number;
    readonly type: string;
    readonly props: Props;
    readonly publicId?: string;
    readonly eventProps: EventProps;
    readonly fiber: any;
    readonly children: HostNode[];
};
type ChildSet = HostNode[] & { owner: object; surfaceId: 0 };
type CommittedHost = { node: HostNode; parentId: number; publicId?: string; eventTarget: any };
type DispatchEventFn = (target: any, topLevelType: string, nativeEventParam: any) => void;
type Event = [number, string, any];
const increment = (value: number) => Math.min(Number.MAX_SAFE_INTEGER, value + 1);

// Copy JSON-facing values so a prospective clone cannot mutate nested props in
// a committed description. Event functions have their own immutable snapshot.
function copyValue(value: any): any {
    if (Array.isArray(value)) return Object.freeze(value.map(copyValue));
    if (value && typeof value === "object") {
        if (typeof value.toJSON === "function") return copyValue(value.toJSON());
        return Object.freeze(Object.fromEntries(Object.entries(value).map(([key, item]) => [key, copyValue(item)])));
    }
    return value;
}
function nativeProps(payload: Record<string, any> | null): Props {
    const { id, children, elementType, type, ...props } = payload ?? {};
    return copyValue(props);
}

export default class {
    readonly unstable_DiscreteEventPriority = 1;
    readonly unstable_ContinuousEventPriority = 2;
    readonly unstable_IdleEventPriority = 3;
    wasmModule?: any;
    dispatchEventFn?: DispatchEventFn;
    readonly fiberNodesMap = new Map<number, any>();
    widgetRegistrationService?: WidgetRegistrationService;
    readonly eventSubject = new Subject<Event>();
    readonly eventSubjectSubscription: Subscription;
    private disposed = false;
    private droppedEvents = 0;
    private pendingEvents: Event[] = [];
    private eventDrainScheduled = false;
    private readonly descriptionOwner = {};
    private readonly surface = { id: 0 as const, revision: undefined as string | undefined,
        committed: new Map<number, CommittedHost>(), failure: undefined as string | undefined };
    private publications = 0;
    private appliedPublications = 0;
    private failedPublications = 0;
    private terminalTeardowns = 0;
    private structuralCalls = 0;
    private observedCreates = 0;
    private observedClones = 0;
    private stagingNodeCount = 0;
    private stagingHighWater = 0;
    private stagingMs = 0;
    private lastPublication: null | { status: string; nodeCount: number; operationCount: number;
        diffMs: number; serializationMs: number; boundaryMs: number; totalMs: number;
        wireBytes: number; nativeRevision?: string; error?: string } = null;

    constructor() {
        this.eventSubjectSubscription = this.eventSubject.subscribe(([id, type, payload]) => {
            const target = this.fiberNodesMap.get(id);
            if (!this.disposed && !this.surface.failure && target !== undefined && this.isTargetAlive(id) && this.dispatchEventFn)
                this.dispatchEventFn(target, type, payload);
            else this.droppedEvents = increment(this.droppedEvents);
        });
    }

    destroy() {
        if (this.disposed) return;
        this.disposed = true;
        this.eventSubjectSubscription.unsubscribe();
        this.eventSubject.complete();
        this.pendingEvents = [];
        this.fiberNodesMap.clear();
        this.surface.committed.clear();
        this.widgetRegistrationService?.destroy();
        this.widgetRegistrationService = undefined;
        this.wasmModule = undefined;
        this.dispatchEventFn = undefined;
    }
    getDiagnostics() {
        return { fiberIds: [...this.fiberNodesMap.keys()].sort((a, b) => a - b), fiberCount: this.fiberNodesMap.size,
            subscriptionClosed: this.eventSubjectSubscription.closed, droppedEvents: this.droppedEvents,
            pendingEventCount: this.pendingEvents.length, surfaceId: this.surface.id,
            committedDescriptionCount: this.surface.committed.size, stagingNodeCount: this.stagingNodeCount,
            // No bridge-owned candidate collection exists between publications.
            // Descriptions retained by pending Fabric work are renderer-owned.
            retainedCandidateCount: 0, stagingHighWater: this.stagingHighWater, stagingMs: this.stagingMs,
            observedCreates: this.observedCreates, observedClones: this.observedClones,
            publications: this.publications, appliedPublications: this.appliedPublications,
            failedPublications: this.failedPublications, structuralCalls: this.structuralCalls,
            terminalTeardowns: this.terminalTeardowns,
            nativeRevision: this.surface.revision, publicationFailure: this.surface.failure,
            lastPublication: this.lastPublication ? { ...this.lastPublication } : null };
    }
    init(wasmModule: any, widgetRegistrationService: WidgetRegistrationService) {
        if (this.disposed || this.wasmModule) throw new Error("Fabric bridge is disposed or already initialized");
        if (typeof wasmModule?.applyCommit !== "function" || typeof wasmModule?.getCommitState !== "function")
            throw new Error("Fabric requires the schema-v2 native publication binding");
        this.wasmModule = wasmModule;
        this.widgetRegistrationService = widgetRegistrationService;
        // Node init can precede native ready. The first publication reads the
        // base revision after ready; later publications use their acknowledgment.
    }
    assertPublicationHealthy = () => {
        if (this.surface.failure) throw new Error(this.surface.failure);
        if (this.disposed || !this.wasmModule) throw new Error("Native bridge is disposed or uninitialized");
    };
    private requireSurface(container: number) {
        if (container !== this.surface.id) throw new Error(`Unsupported Fabric surface: ${container}`);
    }
    private requireNode(node: HostNode) {
        if (node.owner !== this.descriptionOwner || node.surfaceId !== this.surface.id)
            throw new Error("Host description belongs to another Fabric bridge or surface");
    }
    dispatchEvent = (id: number, type: string, payload: any) => {
        if (this.disposed) { this.droppedEvents = increment(this.droppedEvents); return; }
        this.eventSubject.next([id, type, payload]);
    };
    enqueueEvent = (id: number, type: string, payload: any) => {
        if (this.disposed || this.surface.failure || this.pendingEvents.length >= 256) {
            this.droppedEvents = increment(this.droppedEvents); return;
        }
        this.pendingEvents.push([id, type, payload]);
        if (this.eventDrainScheduled) return;
        this.eventDrainScheduled = true;
        queueMicrotask(() => {
            this.eventDrainScheduled = false;
            const events = this.pendingEvents;
            this.pendingEvents = [];
            for (const event of events) this.dispatchEvent(...event);
        });
    };
    acknowledgeDestruction = (destroyedIds: readonly number[]) => {
        if (this.disposed) return;
        for (const id of destroyedIds) {
            this.fiberNodesMap.delete(id);
            this.widgetRegistrationService?.releaseNativeTarget(id);
        }
    };
    private isTargetAlive(id: number) {
        if (this.wasmModule?.isElementAlive && !this.wasmModule.isElementAlive(id)) {
            this.acknowledgeDestruction([id]); return false;
        }
        return this.fiberNodesMap.has(id);
    }
    unstable_getCurrentEventPriority = () => null;
    dispatchCommand = () => {};
    sendAccessibilityEvent = () => {};
    setIsJSResponder = () => {};
    createNode = (id: number, uiViewClassName: string, container: number, payload: Record<string, any> | null, fiber: any): HostNode => {
        this.requireSurface(container);
        if (!Number.isInteger(id) || id < 1 || id > 2147483647) throw new Error("Invalid Fabric native ID");
        const start = performance.now();
        const node: HostNode = { owner: this.descriptionOwner, surfaceId: 0, id,
            type: typeof fiber?.type === "string" ? fiber.type : uiViewClassName,
            props: nativeProps(payload), publicId: typeof payload?.id === "string" ? payload.id : undefined,
            eventProps: readEventProps(payload) ?? captureEventProps(fiber?.pendingProps ?? payload ?? {}),
            fiber, children: [] };
        this.observedCreates = increment(this.observedCreates);
        this.stagingMs += performance.now() - start;
        return node;
    };
    private clone(node: HostNode, props: Record<string, any> | null, replaceChildren: boolean): HostNode {
        this.requireNode(node);
        const start = performance.now();
        const changes = props ? { ...nativeProps(props) } : null;
        if (changes && Object.prototype.hasOwnProperty.call(changes, "root")) {
            if (changes.root != null && typeof changes.root !== "boolean")
                throw new Error("Root metadata must be boolean");
            if (changes.root != null && Boolean(changes.root) !== Boolean(node.props.root))
                throw new Error("Root metadata belongs to the native lifetime; remount to change it");
            // Root is creation metadata, including when a React prop is removed.
            delete changes.root;
        }
        const next: HostNode = { ...node, props: changes ? Object.freeze({ ...node.props, ...changes }) : node.props,
            publicId: props && Object.prototype.hasOwnProperty.call(props, "id")
                ? typeof props.id === "string" ? props.id : undefined : node.publicId,
            eventProps: readEventProps(props) ?? (props ? Object.freeze({ ...node.eventProps, ...captureEventProps(props) }) : node.eventProps),
            children: replaceChildren ? [] : [...node.children] };
        this.observedClones = increment(this.observedClones);
        this.stagingMs += performance.now() - start;
        return next;
    }
    cloneNode = (node: HostNode) => this.clone(node, null, false);
    cloneNodeWithNewProps = (node: HostNode, props: Record<string, any>) => this.clone(node, props, false);
    cloneNodeWithNewChildren = (node: HostNode) => this.clone(node, null, true);
    cloneNodeWithNewChildrenAndProps = (node: HostNode, props: Record<string, any>) => this.clone(node, props, true);
    // RN's persistence host contract calls createChildSet without arguments.
    // Container ownership is checked at createNode and completeRoot as well.
    createChildSet = (container = 0): ChildSet => {
        this.requireSurface(container);
        return Object.assign([], { owner: this.descriptionOwner, surfaceId: 0 as const });
    };
    appendChildToSet = (set: ChildSet, child: HostNode) => {
        if (set.owner !== this.descriptionOwner || set.surfaceId !== 0) throw new Error("Invalid Fabric child set");
        this.requireNode(child); set.push(child);
    };
    appendChild = (parent: HostNode, child: HostNode) => {
        this.requireNode(parent); this.requireNode(child);
        parent.children.push(child);
    };
    completeRoot = (container: number, newChildSet: ChildSet) => {
        this.requireSurface(container);
        // React may unmount after a commit-phase failure. Allow only release of
        // its empty host tree; this is not a native publication or recovery.
        // Completion checks continue to report the original terminal failure.
        if (this.surface.failure && newChildSet.owner === this.descriptionOwner && newChildSet.surfaceId === 0 && newChildSet.length === 0) {
            this.terminalTeardowns = increment(this.terminalTeardowns);
            return;
        }
        this.assertPublicationHealthy();
        this.publications = increment(this.publications);
        const start = performance.now();
        let diffMs = 0, serializationMs = 0, boundaryMs = 0, wireBytes = 0, operationCount = 0;
        let status = "failed";
        const candidate = new Map<number, CommittedHost>();
        try {
            if (newChildSet.owner !== this.descriptionOwner || newChildSet.surfaceId !== 0) throw new Error("Invalid Fabric child set");
            const operations: NativeCommitOperation[] = [];
            const publicOwners = new Map<string, number>();
            const pending = [...newChildSet].reverse().map(node => ({ node, parentId: 0 }));
            while (pending.length) {
                const { node, parentId } = pending.pop()!;
                this.requireNode(node);
                if (candidate.has(node.id)) throw new Error(`Duplicate/cyclic Fabric ownership for ${node.id}`);
                candidate.set(node.id, { node, parentId, eventTarget: undefined });
                if (node.publicId !== undefined) publicOwners.set(node.publicId, node.id);
                this.stagingNodeCount = candidate.size;
                this.stagingHighWater = Math.max(this.stagingHighWater, candidate.size);
                const previous = this.surface.committed.get(node.id)?.node;
                if (!previous) operations.push({ op: "create", id: node.id, elementType: node.type, props: node.props });
                else {
                    if (previous.type !== node.type) throw new Error("A native lifetime cannot change element type");
                    const patch: Record<string, unknown> = {};
                    for (const key of new Set([...Object.keys(previous.props), ...Object.keys(node.props)])) {
                        if (key !== "root" && previous.props[key] !== node.props[key]) patch[key] = node.props[key] ?? null;
                    }
                    if (Object.keys(patch).length) operations.push({ op: "patch", id: node.id, props: patch });
                }
                operations.push({ op: "setChildren", parentId: node.id, childrenIds: node.children.map(child => child.id) });
                for (let i = node.children.length - 1; i >= 0; --i) pending.push({ node: node.children[i], parentId: node.id });
                Object.freeze(node.children); Object.freeze(node);
            }
            // Fabric mutates its canonical.currentProps during render. Event
            // targets own a committed host-only ancestry and canonical props;
            // the original Fiber remains the lifetime's prototype/handle.
            for (const host of candidate.values()) {
                const { node, parentId } = host;
                const canonical = node.fiber?.stateNode?.canonical ?? {};
                host.eventTarget = Object.assign(Object.create(node.fiber ?? null), {
                    tag: 5, alternate: null, return: candidate.get(parentId)?.eventTarget ?? null,
                    stateNode: { node, canonical: { ...canonical, currentProps: node.eventProps } },
                });
                host.publicId = node.publicId !== undefined && publicOwners.get(node.publicId) === node.id ? node.publicId : undefined;
            }
            if (this.surface.revision === undefined) {
                const state: NativeCommitState = JSON.parse(this.wasmModule.getCommitState());
                if (state.schemaVersion !== 2 || !state.initialized || state.surfaceStatus !== "healthy" || state.managedCount !== 0)
                    throw new Error("Fabric requires a ready, healthy, unowned native surface using schema v2");
                this.surface.revision = state.nativeRevision;
            }
            diffMs = performance.now() - start;
            const serializationStart = performance.now();
            const wire = JSON.stringify({ schemaVersion: 2, surfaceId: 0, baseRevision: this.surface.revision,
                rootChildren: newChildSet.map(node => node.id), operations });
            wireBytes = new TextEncoder().encode(wire).length;
            serializationMs = performance.now() - serializationStart;
            operationCount = operations.length;
            const boundaryStart = performance.now();
            this.structuralCalls = increment(this.structuralCalls);
            const result: NativeCommitResult = JSON.parse(this.wasmModule.applyCommit(wire));
            boundaryMs = performance.now() - boundaryStart;
            status = result.status;
            if (result.schemaVersion !== 2 || result.surfaceId !== 0 || result.status !== "applied")
                throw new Error(`Native publication ${result.status}: ${result.error?.code ?? "invalid_acknowledgment"}`);
            if (BigInt(result.nativeRevision) !== BigInt(this.surface.revision) + 1n)
                throw new Error("Native publication returned an inconsistent revision");
            const expectedDestroyed = [...this.surface.committed.keys()].filter(id => !candidate.has(id)).sort((a, b) => a - b);
            if (!Array.isArray(result.destroyedIds) || JSON.stringify([...result.destroyedIds].sort((a, b) => a - b)) !== JSON.stringify(expectedDestroyed))
                throw new Error("Native publication returned inconsistent destruction ownership");
            this.acknowledgeDestruction(result.destroyedIds);
            for (const [id, host] of candidate) {
                this.widgetRegistrationService!.createNativeTarget(id);
                if (this.surface.committed.get(id)?.publicId !== host.publicId || !this.surface.committed.has(id))
                    this.widgetRegistrationService!.setPublicId(id, host.publicId);
                this.fiberNodesMap.set(id, host.eventTarget);
            }
            this.surface.committed = candidate;
            this.surface.revision = result.nativeRevision;
            this.appliedPublications = increment(this.appliedPublications);
        } catch (error) {
            if (status === "applied") status = "failed";
            this.failedPublications = increment(this.failedPublications);
            this.surface.failure = `Fabric surface failed: ${error instanceof Error ? error.message : String(error)}`;
            // A commit-phase exception does not stop React from attempting later
            // effects/callbacks. Explicitly fail the bridge and invalidate every
            // usable target; wrapper completion checks must reject success too.
            this.widgetRegistrationService?.destroy();
            this.fiberNodesMap.clear();
            this.surface.committed.clear();
            this.pendingEvents = [];
            throw error;
        } finally {
            this.stagingNodeCount = 0;
            this.lastPublication = { status, nodeCount: candidate.size, operationCount, diffMs, serializationMs,
                boundaryMs, totalMs: performance.now() - start, wireBytes, nativeRevision: this.surface.revision,
                error: this.surface.failure };
        }
    };
    registerEventHandler = (dispatchEventFn: DispatchEventFn) => { this.dispatchEventFn = dispatchEventFn; };
}
