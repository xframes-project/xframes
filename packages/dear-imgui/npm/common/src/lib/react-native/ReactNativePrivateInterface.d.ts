type AttributePayload = Record<string, unknown> | null;
type ValidAttributes = Record<string, unknown>;

declare const ReactNativePrivateInterface: {
    createAttributePayload(
        props: Record<string, unknown>,
        validAttributes: ValidAttributes,
    ): AttributePayload;
    diffAttributePayloads(
        previousProps: Record<string, unknown>,
        nextProps: Record<string, unknown>,
        validAttributes: ValidAttributes,
    ): AttributePayload;
    flattenStyle(style: unknown): Record<string, unknown> | undefined;
    createPublicRootInstance(containerTag: number): { containerTag: number };
    nativeFabricUIManager: {
        init(nativeModule: unknown, widgetRegistrationService: unknown): void;
        dispatchEvent(rootNodeId: number, topLevelType: string, nativeEvent: unknown): void;
        unstable_getCurrentEventPriority(): number | null;
    };
};

export default ReactNativePrivateInterface;
