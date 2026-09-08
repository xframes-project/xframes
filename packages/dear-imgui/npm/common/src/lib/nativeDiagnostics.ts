import type { NativeCounter } from "./nativeCommit";

export type NativeFrameReason = "initial" | "publication" | "imperative" | "input" | "window" | "resource"
    | "screenshot" | "style" | "diagnostics" | "layout" | "cursor" | "keyRepeat" | "hover" | "interaction"
    | "canvas" | "map" | "retry";
export interface NativeFrameCorrelation {
    frameId: NativeCounter;
    nativeRevision: NativeCounter;
    coveredGeneration: NativeCounter;
    /** Fixed reason bitset; this is not an ordering counter. */
    reasons: number;
    capturedAtMs: number;
    /** Backend submission endpoint, never physical presentation. */
    submittedAtMs: number;
}
export interface NativeFrameSchedulerState {
    status: "running" | "quarantined" | "exhausted" | "disposed" | "backendFailed";
    renderable: boolean;
    generation: NativeCounter;
    coveredGeneration: NativeCounter;
    dirty: boolean;
    waiting: boolean;
    notificationPending: boolean;
    wakeAttached: boolean;
    ownerCount: number;
    activeOwners: number;
    deadlines: number;
    nextDeadlineMs: number | null;
    deferredUntilMs: number | null;
    ownerHighWater: NativeCounter;
    rejectedOwners: NativeCounter;
    wakeCount: NativeCounter;
    opportunities: NativeCounter;
    skippedOpportunities: NativeCounter;
    constructed: NativeCounter;
    submitted: NativeCounter;
    abandoned: NativeCounter;
    metricOverflows: NativeCounter;
    lastInvalidatedAtMs: number;
    lastWakeAtMs: number;
    correlationRecords: number;
    completedFrame: NativeFrameCorrelation | null;
    reasons: Record<NativeFrameReason, {
        invalidations: NativeCounter; pending: boolean; activeOwners: number; deadlines: number; deadlinesFired: NativeCounter;
    }>;
}
export interface NativeFrameSnapshot extends Omit<NativeFrameCorrelation, "capturedAtMs"> {
    constructedAtMs: number;
    elementCount: number;
    hierarchyCount: number;
    internalSubjectCount: number;
    unreachableCount: number;
    vertices: number;
    elements: { id: number; type: string; children: number[]; yogaChildren: number[]; yogaParent: number | null;
        lastInternalOpMs: number | null; bounds: number[]; state?: any;
        resources?: { loadedTextures: number; queuedLoads: number; pendingRequests: number; lastLoadFailed?: boolean;
            scriptReady?: boolean; prefetchCompleted?: number; prefetchTotal?: number; queuedPrefetch?: number; failedTiles?: number } }[];
}
/** Scheduler state is always available. A snapshot appears only after an enabled
 * frame submits; disabling diagnostics retains it without relabelling its coverage. */
export type NativeDiagnostics = Partial<NativeFrameSnapshot> & {
    enabled: boolean;
    sampledAtMs: number;
    surfaceStatus: "healthy" | "quarantined" | "disposed";
    scheduler: NativeFrameSchedulerState;
    platform: { windowCallbacks: number; animationCallbacks: number; deadlineTimers: number;
        visibilityListeners: number; pendingScreenshots: number };
    resourceState: { textures: { liveTextures: number; retiredTextures: number }; queuedPrefetchEvents: number;
        mapWorkers?: { queued: number; active: number; threads: number; stopped: boolean; capacity: number } };
};
export interface NativeDiagnosticsBinding {
    setDiagnosticsEnabled(enabled: boolean): void;
    getDiagnostics(): string;
}
