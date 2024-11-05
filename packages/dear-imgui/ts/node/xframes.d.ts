declare module 'xframes' {
    // Add your type definitions here
    export interface XFrames {
        init: (assetsBasePath: string, jsonStringifiedFontDefs: string, jsonStringifiedThemeOverrides: string) => void;
        showDebugWindow: () => void;
        setElement: (jsonStringifiedElementDefinition: string) => void;
        patchElement: (widgetId: number, jsonStringifiedElementPatchDefinition: string) => void;
        setChildren: (parentId: number, childrenIds: number[]) => void;
        appendChild: (parentId: number, childId: number) => void;
    }
    const xframes: XFrames;
    export default xframes;
}