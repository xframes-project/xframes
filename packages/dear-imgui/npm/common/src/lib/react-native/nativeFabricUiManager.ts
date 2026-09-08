import { Subject, Subscription } from "rxjs";
// import { MainModule } from "../wasm/wasm-app-types";
import { WidgetRegistrationService } from "../widgetRegistrationService";
import type { NativeCommit, NativeCommitResult } from "../nativeCommit";

type CloningNode = { id: number; childrenIds: number[] } | null;
type DispatchEventFn = (id: number, topLevelType: string, nativeEventParam: any) => void;
type Event = [number, string, any];

export default class {
    readonly unstable_DiscreteEventPriority = 1;
    readonly unstable_ContinuousEventPriority = 2;
    readonly unstable_IdleEventPriority = 3;
    wasmModule?: any;
    dispatchEventFn?: DispatchEventFn;
    cloningNode?: CloningNode;
    fiberNodesMap: Map<number, any>;
    widgetRegistrationService?: WidgetRegistrationService;
    eventSubject: Subject<Event>;
    eventSubjectSubscription: Subscription;
    private disposed = false;
    private droppedEvents = 0;
    private pendingEvents: Event[] = [];
    private eventDrainScheduled = false;

    linkedWidgetTypes: string[] = [
        "di-button",
        "checkbox",
        "clipped-multi-line-text-renderer",
        "collapsing-header",
        "combo",
        "di-image",
        "input-text",
        "map-view",
        "multi-slider",
        "plot-line",
        "plot-candlestick",
        "slider",
        "tree-node",
        "tab-bar",
        "tab-item",
        "di-table",
        "di-js-canvas",
        "di-lua-canvas",
        "di-janet-canvas",
    ];

    constructor() {
        this.fiberNodesMap = new Map();
        this.eventSubject = new Subject<Event>();

        this.eventSubjectSubscription = this.eventSubject.subscribe(
            ([rootNodeID, topLevelType, nativeEventParam]) => {
                // Native event queues are independent. Check at dispatch, including
                // events queued before deletion and delivered after acknowledgment.
                const fiber = this.fiberNodesMap.get(rootNodeID);
                if (!this.disposed && fiber !== undefined && this.isTargetAlive(rootNodeID) && this.dispatchEventFn) {
                    this.dispatchEventFn(
                        fiber,
                        topLevelType,
                        nativeEventParam,
                    );
                } else this.droppedEvents = Math.min(Number.MAX_SAFE_INTEGER, this.droppedEvents + 1);
            },
        );
    }

    destroy() {
        if (this.disposed) return;
        this.disposed = true;
        this.eventSubjectSubscription.unsubscribe();
        this.eventSubject.complete();
        this.pendingEvents = [];
        this.fiberNodesMap.clear();
        this.cloningNode = null;
        this.widgetRegistrationService?.destroy();
        this.widgetRegistrationService = undefined;
        this.wasmModule = undefined;
        this.dispatchEventFn = undefined;
    }

    // Opt-in inspection only: does not retain Fiber objects or alter their lifetime.
    getDiagnostics() {
        return {
            fiberIds: [...this.fiberNodesMap.keys()].sort((a, b) => a - b),
            fiberCount: this.fiberNodesMap.size,
            subscriptionClosed: this.eventSubjectSubscription.closed,
            droppedEvents: this.droppedEvents,
            pendingEventCount: this.pendingEvents.length,
        };
    }

    init(wasmModule: any, widgetRegistrationService: WidgetRegistrationService) {
        if (this.disposed) throw new Error("Cannot initialize a disposed Fabric bridge");
        this.wasmModule = wasmModule;
        this.widgetRegistrationService = widgetRegistrationService;
    }
    dispatchEvent = (rootNodeID: number, topLevelType: string, nativeEventParam: any) => {
        if (this.disposed) {
            this.droppedEvents = Math.min(Number.MAX_SAFE_INTEGER, this.droppedEvents + 1);
            return;
        }
        this.eventSubject.next([rootNodeID, topLevelType, nativeEventParam]);
    };
    // Wasm callbacks originate inside native rendering. Copy their scalar payload
    // and defer Fabric/liveness access until that native stack releases tree locks.
    enqueueEvent = (id: number, type: string, payload: any) => {
        if (this.disposed || this.pendingEvents.length >= 256) {
            this.droppedEvents = Math.min(Number.MAX_SAFE_INTEGER, this.droppedEvents + 1);
            return;
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
            if (this.cloningNode?.id === id) this.cloningNode = null;
        }
    };
    applyCommit = (commit: NativeCommit): NativeCommitResult => {
        if (this.disposed || !this.wasmModule) throw new Error("Native bridge is disposed or uninitialized");
        const result: NativeCommitResult = JSON.parse(this.wasmModule.applyCommit(JSON.stringify(commit)));
        // Also release completed destructions from an explicitly failed partial apply.
        // Rejected batches always return an empty list. No snapshot/registry reset is needed.
        this.acknowledgeDestruction(result.destroyedIds);
        return result;
    };
    private isTargetAlive(id: number) {
        if (this.wasmModule?.isElementAlive && !this.wasmModule.isElementAlive(id)) {
            this.acknowledgeDestruction([id]);
            return false;
        }
        return this.fiberNodesMap.has(id);
    }
    private setNativeChildren(id: number, payload: string) {
        const result = this.wasmModule?.setChildren(id, payload);
        // Older direct bindings may ignore/omit results. Current Node and Wasm
        // return the same JSON array after synchronous subject application.
        if (result !== undefined) this.acknowledgeDestruction(JSON.parse(result));
    }
    unstable_getCurrentEventPriority = () => null;
    dispatchCommand = () => {};
    sendAccessibilityEvent = () => {};
    setIsJSResponder = () => {};
    createNode = (
        generatedId: number,
        uiViewClassName: string,
        requiresClone: boolean,
        payload: Record<string, any> | null,
        fiberNode: any,
    ) => {
        if (this.disposed) return { id: generatedId, type: fiberNode?.type ?? "node" };
        // todo: yikes
        if (this.cloningNode) {
            this.cloningNode = null;
        }

        let element: any = { id: generatedId };
        this.widgetRegistrationService?.createNativeTarget(generatedId);

        // console.log("createNode", generatedId, uiViewClassName, requiresClone, payload, fiberNode);

        if (payload) {
            const { children, id, elementType, ...props } = payload;
            const { type } = fiberNode;

            element = {
                ...element,
                ...props,
                type,
            };

            // todo: is there a good reason why we shouldn't keep track of all widgets?
            // if (this.linkedWidgetTypes.includes(type)) {
            if (typeof id === "string") {
                this.widgetRegistrationService?.linkWidgetIds(id, generatedId);
            }
        } else {
            element.type = "node";
        }

        // console.log(JSON.stringify(element));

        this.fiberNodesMap.set(generatedId, fiberNode);
        this.wasmModule?.setElement(JSON.stringify(element));

        // console.log(fiberNode);

        return element;
    };
    cloneNodeWithNewProps = (node: any, newProps: any) => {
        const newWidget = this.cloneProps(node, newProps);
        this.wasmModule?.patchElement(node.id, JSON.stringify(newWidget));

        return newWidget;
    };
    cloneNodeWithNewChildrenAndProps = (node: any, newProps: any) => {
        const newWidget = this.cloneProps(node, newProps);
        this.wasmModule?.patchElement(node.id, JSON.stringify(newWidget));

        if (this.cloningNode) {
            this.setNativeChildren(
                this.cloningNode.id,
                JSON.stringify(this.cloningNode.childrenIds),
            );
        }
        this.cloningNode = { id: newWidget.id, childrenIds: [] };

        return newWidget;
    };
    // This is a rather problematic method
    // I should drop all children then re-append them but naturally this causes out of memory bounds issues
    // Also, I still don't understand at which point I am supposed to delete widgets/nodes
    // todo: have a look at react-native's own implementation of this...
    cloneNodeWithNewChildren = (node: any) => {
        // todo: yikes
        if (this.cloningNode) {
            this.setNativeChildren(
                this.cloningNode.id,
                JSON.stringify(this.cloningNode.childrenIds),
            );
        }

        this.cloningNode = { id: node.id, childrenIds: [] };

        // todo: does this make sense?
        return node;
    };
    createChildSet(...args: any[]) {
        // console.log("createChildSet", args);

        return [];
    }
    appendChildToSet(set: any[], child: any) {
        // console.log("appendChildToSet", set, child);

        set.push(child);
    }
    appendChild = (parent: any, child: any) => {
        // console.log("appendChild", parent, child);

        // todo: yikes
        if (this.cloningNode) {
            this.cloningNode.childrenIds.push(child.id);
        } else {
            this.wasmModule?.appendChild(parent.id, child.id);
        }
    };
    completeRoot = (container: any, newChildSet: any) => {
        // console.log("completeRoot", container, newChildSet);

        // todo: yikes
        if (this.cloningNode) {
            const cloningNodeId = this.cloningNode.id;
            const payload = JSON.stringify(this.cloningNode.childrenIds);
            this.setNativeChildren(cloningNodeId, payload);
            this.cloningNode = null;
        }

        const payload = JSON.stringify(newChildSet.map(({ id }: { id: number }) => id));

        this.setNativeChildren(container, payload);
    };
    private cloneProps(node: any, newProps: any) {
        const { id, children, elementType, ...props } = newProps ?? {};
        if (newProps && Object.prototype.hasOwnProperty.call(newProps, "id")) {
            this.widgetRegistrationService?.setPublicId(node.id, id);
        }
        return { ...node, ...props, id: node.id };
    }
    registerEventHandler = (dispatchEventFn: DispatchEventFn) => {
        this.dispatchEventFn = dispatchEventFn;
    };
}
