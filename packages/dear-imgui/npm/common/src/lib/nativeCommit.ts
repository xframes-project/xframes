/** Stage 2 native structural transactions. These are not Fabric commits or frame revisions. */
export type NativeCommitOperation =
    | { op: "create"; id: number; elementType: string; props: Record<string, unknown> }
    | { op: "patch"; id: number; props: Record<string, unknown> }
    | { op: "setChildren"; parentId: number; childrenIds: number[] }
    | { op: "appendChild"; parentId: number; childId: number };

export interface NativeCommit {
    schemaVersion: 1;
    /** The current Fabric container and sole native surface are both 0. */
    surfaceId: 0;
    /** Optional opaque correlation, at most 128 UTF-8 bytes; never controls ordering. */
    correlationId?: string;
    operations: NativeCommitOperation[];
}

/** Unsigned 64-bit decimal string. Use BigInt when arithmetic is needed. */
export type NativeCounter = string;
export type NativeCommitErrorCode =
    | "invalid_json" | "missing_field" | "invalid_field" | "unknown_field"
    | "unsupported_version" | "unsupported_surface" | "unsupported_operation"
    | "invalid_id" | "invalid_element_type" | "invalid_props" | "immutable_identity"
    | "duplicate_id" | "duplicate_child" | "missing_target" | "destroyed_id"
    | "cycle" | "multiple_parents" | "invalid_relationship" | "counter_overflow"
    | "application_error" | "runtime_not_ready";

interface NativeCommitResultBase {
    schemaVersion: 1;
    surfaceId: 0;
    nativeRevision: NativeCounter;
    correlationId?: string;
    destroyedIds: number[];
}
export type NativeCommitResult = NativeCommitResultBase & (
    | { status: "applied"; nativeSequence: NativeCounter; error?: never }
    | { status: "rejected"; nativeSequence: null; error: NativeCommitError }
    | { status: "failed"; nativeSequence: NativeCounter; error: NativeCommitError }
);
export interface NativeCommitError {
    code: NativeCommitErrorCode;
    message: string;
    operationIndex: number | null;
}
export interface NativeCommitState {
    schemaVersion: 1;
    surfaceId: 0;
    initialized: boolean;
    nativeSequence: NativeCounter;
    nativeRevision: NativeCounter;
    lastTransaction?: null | {
        nativeSequence?: NativeCounter | null;
        nativeRevision?: NativeCounter;
        operationCount?: number;
        compatibility?: boolean;
        validationMs?: number;
        applicationMs?: number;
        envelopeMs?: number;
        dispatchMs?: number;
        operationIndex?: number | null;
        parseMs?: number;
        totalMs?: number;
        wireBytes?: number;
        status: NativeCommitResult["status"];
        errorCode: NativeCommitErrorCode | null;
    };
}

/** Both actual bindings accept/return JSON strings, synchronously. */
export interface NativeTransactionBinding {
    applyCommit(payload: string): string;
    getCommitState(): string;
}
