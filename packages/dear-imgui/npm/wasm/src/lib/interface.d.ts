// TypeScript bindings for emscripten-generated code.  Automatically generated at compile time.
declare namespace RuntimeExports {
    let HEAPF32: any;
    let HEAPF64: any;
    let HEAP_DATA_VIEW: any;
    let HEAP8: any;
    let HEAPU8: any;
    let HEAP16: any;
    let HEAPU16: any;
    let HEAP32: any;
    let HEAPU32: any;
    let HEAP64: any;
    let HEAPU64: any;
    let FS_createPath: any;
    function FS_createDataFile(parent: any, name: any, fileData: any, canRead: any, canWrite: any, canOwn: any): void;
    function FS_createPreloadedFile(parent: any, name: any, url: any, canRead: any, canWrite: any, onload: any, onerror: any, dontCreateFile: any, canOwn: any, preFinish: any): void;
    function FS_unlink(path: any): any;
    let FS_createLazyFile: any;
    let FS_createDevice: any;
    let addRunDependency: any;
    let removeRunDependency: any;
}
interface WasmModule {
  _main(_0: number, _1: number): number;
  ___set_stack_limits(_0: number, _1: number): void;
}

type EmbindString = ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string;
export interface ImGuiWindowFlagsValue<T extends number> {
  value: T;
}
export type ImGuiWindowFlags = ImGuiWindowFlagsValue<0>|ImGuiWindowFlagsValue<1>|ImGuiWindowFlagsValue<2>|ImGuiWindowFlagsValue<4>|ImGuiWindowFlagsValue<8>|ImGuiWindowFlagsValue<16>|ImGuiWindowFlagsValue<32>|ImGuiWindowFlagsValue<64>|ImGuiWindowFlagsValue<128>|ImGuiWindowFlagsValue<256>|ImGuiWindowFlagsValue<512>|ImGuiWindowFlagsValue<1024>|ImGuiWindowFlagsValue<2048>|ImGuiWindowFlagsValue<4096>|ImGuiWindowFlagsValue<8192>|ImGuiWindowFlagsValue<16384>|ImGuiWindowFlagsValue<32768>|ImGuiWindowFlagsValue<65536>|ImGuiWindowFlagsValue<131072>|ImGuiWindowFlagsValue<262144>|ImGuiWindowFlagsValue<196608>|ImGuiWindowFlagsValue<43>|ImGuiWindowFlagsValue<197120>|ImGuiWindowFlagsValue<1073741824>;

export interface ImGuiChildFlagsValue<T extends number> {
  value: T;
}
export type ImGuiChildFlags = ImGuiChildFlagsValue<0>|ImGuiChildFlagsValue<1>|ImGuiChildFlagsValue<2>|ImGuiChildFlagsValue<4>|ImGuiChildFlagsValue<8>|ImGuiChildFlagsValue<16>|ImGuiChildFlagsValue<32>|ImGuiChildFlagsValue<64>|ImGuiChildFlagsValue<128>;

export interface ImGuiInputTextFlagsValue<T extends number> {
  value: T;
}
export type ImGuiInputTextFlags = ImGuiInputTextFlagsValue<0>|ImGuiInputTextFlagsValue<1>|ImGuiInputTextFlagsValue<2>|ImGuiInputTextFlagsValue<4>|ImGuiInputTextFlagsValue<8>|ImGuiInputTextFlagsValue<16>|ImGuiInputTextFlagsValue<32>|ImGuiInputTextFlagsValue<64>|ImGuiInputTextFlagsValue<128>|ImGuiInputTextFlagsValue<256>|ImGuiInputTextFlagsValue<512>|ImGuiInputTextFlagsValue<1024>|ImGuiInputTextFlagsValue<2048>|ImGuiInputTextFlagsValue<4096>|ImGuiInputTextFlagsValue<8192>|ImGuiInputTextFlagsValue<16384>|ImGuiInputTextFlagsValue<32768>|ImGuiInputTextFlagsValue<65536>|ImGuiInputTextFlagsValue<131072>|ImGuiInputTextFlagsValue<262144>|ImGuiInputTextFlagsValue<524288>|ImGuiInputTextFlagsValue<1048576>;

export interface ImGuiTreeNodeFlagsValue<T extends number> {
  value: T;
}
export type ImGuiTreeNodeFlags = ImGuiTreeNodeFlagsValue<0>|ImGuiTreeNodeFlagsValue<1>|ImGuiTreeNodeFlagsValue<2>|ImGuiTreeNodeFlagsValue<4>|ImGuiTreeNodeFlagsValue<8>|ImGuiTreeNodeFlagsValue<16>|ImGuiTreeNodeFlagsValue<32>|ImGuiTreeNodeFlagsValue<64>|ImGuiTreeNodeFlagsValue<128>|ImGuiTreeNodeFlagsValue<256>|ImGuiTreeNodeFlagsValue<512>|ImGuiTreeNodeFlagsValue<1024>|ImGuiTreeNodeFlagsValue<2048>|ImGuiTreeNodeFlagsValue<4096>|ImGuiTreeNodeFlagsValue<8192>|ImGuiTreeNodeFlagsValue<16384>|ImGuiTreeNodeFlagsValue<32768>|ImGuiTreeNodeFlagsValue<26>|ImGuiTreeNodeFlagsValue<4>;

export interface ImGuiPopupFlagsValue<T extends number> {
  value: T;
}
export type ImGuiPopupFlags = ImGuiPopupFlagsValue<0>|ImGuiPopupFlagsValue<0>|ImGuiPopupFlagsValue<1>|ImGuiPopupFlagsValue<2>|ImGuiPopupFlagsValue<31>|ImGuiPopupFlagsValue<1>|ImGuiPopupFlagsValue<32>|ImGuiPopupFlagsValue<128>|ImGuiPopupFlagsValue<256>|ImGuiPopupFlagsValue<1024>|ImGuiPopupFlagsValue<2048>|ImGuiPopupFlagsValue<3072>;

export interface ImGuiSelectableFlagsValue<T extends number> {
  value: T;
}
export type ImGuiSelectableFlags = ImGuiSelectableFlagsValue<0>|ImGuiSelectableFlagsValue<1>|ImGuiSelectableFlagsValue<2>|ImGuiSelectableFlagsValue<4>|ImGuiSelectableFlagsValue<8>|ImGuiSelectableFlagsValue<16>|ImGuiSelectableFlagsValue<16>;

export interface ImGuiComboFlagsValue<T extends number> {
  value: T;
}
export type ImGuiComboFlags = ImGuiComboFlagsValue<0>|ImGuiComboFlagsValue<1>|ImGuiComboFlagsValue<2>|ImGuiComboFlagsValue<4>|ImGuiComboFlagsValue<8>|ImGuiComboFlagsValue<16>|ImGuiComboFlagsValue<32>|ImGuiComboFlagsValue<64>|ImGuiComboFlagsValue<128>|ImGuiComboFlagsValue<30>;

export interface ImGuiTabBarFlagsValue<T extends number> {
  value: T;
}
export type ImGuiTabBarFlags = ImGuiTabBarFlagsValue<0>|ImGuiTabBarFlagsValue<1>|ImGuiTabBarFlagsValue<2>|ImGuiTabBarFlagsValue<4>|ImGuiTabBarFlagsValue<8>|ImGuiTabBarFlagsValue<16>|ImGuiTabBarFlagsValue<32>|ImGuiTabBarFlagsValue<64>|ImGuiTabBarFlagsValue<128>|ImGuiTabBarFlagsValue<192>|ImGuiTabBarFlagsValue<64>;

export interface ImGuiTabItemFlagsValue<T extends number> {
  value: T;
}
export type ImGuiTabItemFlags = ImGuiTabItemFlagsValue<0>|ImGuiTabItemFlagsValue<1>|ImGuiTabItemFlagsValue<2>|ImGuiTabItemFlagsValue<4>|ImGuiTabItemFlagsValue<8>|ImGuiTabItemFlagsValue<16>|ImGuiTabItemFlagsValue<32>|ImGuiTabItemFlagsValue<64>|ImGuiTabItemFlagsValue<128>|ImGuiTabItemFlagsValue<256>;

export interface ImGuiFocusedFlagsValue<T extends number> {
  value: T;
}
export type ImGuiFocusedFlags = ImGuiFocusedFlagsValue<0>|ImGuiFocusedFlagsValue<1>|ImGuiFocusedFlagsValue<2>|ImGuiFocusedFlagsValue<4>|ImGuiFocusedFlagsValue<8>|ImGuiFocusedFlagsValue<3>;

export interface ImGuiHoveredFlagsValue<T extends number> {
  value: T;
}
export type ImGuiHoveredFlags = ImGuiHoveredFlagsValue<0>|ImGuiHoveredFlagsValue<1>|ImGuiHoveredFlagsValue<2>|ImGuiHoveredFlagsValue<4>|ImGuiHoveredFlagsValue<8>|ImGuiHoveredFlagsValue<32>|ImGuiHoveredFlagsValue<128>|ImGuiHoveredFlagsValue<256>|ImGuiHoveredFlagsValue<512>|ImGuiHoveredFlagsValue<1024>|ImGuiHoveredFlagsValue<2048>|ImGuiHoveredFlagsValue<768>|ImGuiHoveredFlagsValue<928>|ImGuiHoveredFlagsValue<3>|ImGuiHoveredFlagsValue<4096>|ImGuiHoveredFlagsValue<8192>|ImGuiHoveredFlagsValue<16384>|ImGuiHoveredFlagsValue<32768>|ImGuiHoveredFlagsValue<65536>|ImGuiHoveredFlagsValue<131072>;

export interface ImGuiDragDropFlagsValue<T extends number> {
  value: T;
}
export type ImGuiDragDropFlags = ImGuiDragDropFlagsValue<0>|ImGuiDragDropFlagsValue<1>|ImGuiDragDropFlagsValue<2>|ImGuiDragDropFlagsValue<4>|ImGuiDragDropFlagsValue<8>|ImGuiDragDropFlagsValue<16>|ImGuiDragDropFlagsValue<32>|ImGuiDragDropFlagsValue<1024>|ImGuiDragDropFlagsValue<2048>|ImGuiDragDropFlagsValue<4096>|ImGuiDragDropFlagsValue<3072>;

export interface ImGuiDataTypeValue<T extends number> {
  value: T;
}
export type ImGuiDataType = ImGuiDataTypeValue<0>|ImGuiDataTypeValue<1>|ImGuiDataTypeValue<2>|ImGuiDataTypeValue<3>|ImGuiDataTypeValue<4>|ImGuiDataTypeValue<5>|ImGuiDataTypeValue<6>|ImGuiDataTypeValue<7>|ImGuiDataTypeValue<8>|ImGuiDataTypeValue<9>|ImGuiDataTypeValue<10>;

export interface ImGuiDirValue<T extends number> {
  value: T;
}
export type ImGuiDir = ImGuiDirValue<-1>|ImGuiDirValue<0>|ImGuiDirValue<1>|ImGuiDirValue<2>|ImGuiDirValue<3>|ImGuiDirValue<4>;

export interface ImGuiSortDirectionValue<T extends number> {
  value: T;
}
export type ImGuiSortDirection = ImGuiSortDirectionValue<0>|ImGuiSortDirectionValue<1>|ImGuiSortDirectionValue<2>;

export interface ImGuiKeyValue<T extends number> {
  value: T;
}
export type ImGuiKey = ImGuiKeyValue<0>|ImGuiKeyValue<512>|ImGuiKeyValue<513>|ImGuiKeyValue<514>|ImGuiKeyValue<515>|ImGuiKeyValue<516>|ImGuiKeyValue<517>|ImGuiKeyValue<518>|ImGuiKeyValue<519>|ImGuiKeyValue<520>|ImGuiKeyValue<521>|ImGuiKeyValue<522>|ImGuiKeyValue<523>|ImGuiKeyValue<524>|ImGuiKeyValue<525>|ImGuiKeyValue<526>|ImGuiKeyValue<527>|ImGuiKeyValue<528>|ImGuiKeyValue<529>|ImGuiKeyValue<530>|ImGuiKeyValue<531>|ImGuiKeyValue<532>|ImGuiKeyValue<533>|ImGuiKeyValue<534>|ImGuiKeyValue<535>|ImGuiKeyValue<536>|ImGuiKeyValue<537>|ImGuiKeyValue<538>|ImGuiKeyValue<539>|ImGuiKeyValue<540>|ImGuiKeyValue<541>|ImGuiKeyValue<542>|ImGuiKeyValue<543>|ImGuiKeyValue<544>|ImGuiKeyValue<545>|ImGuiKeyValue<546>|ImGuiKeyValue<547>|ImGuiKeyValue<548>|ImGuiKeyValue<549>|ImGuiKeyValue<550>|ImGuiKeyValue<551>|ImGuiKeyValue<552>|ImGuiKeyValue<553>|ImGuiKeyValue<554>|ImGuiKeyValue<555>|ImGuiKeyValue<556>|ImGuiKeyValue<557>|ImGuiKeyValue<558>|ImGuiKeyValue<559>|ImGuiKeyValue<560>|ImGuiKeyValue<561>|ImGuiKeyValue<562>|ImGuiKeyValue<563>|ImGuiKeyValue<564>|ImGuiKeyValue<565>|ImGuiKeyValue<566>|ImGuiKeyValue<567>|ImGuiKeyValue<568>|ImGuiKeyValue<569>|ImGuiKeyValue<570>|ImGuiKeyValue<571>|ImGuiKeyValue<572>|ImGuiKeyValue<573>|ImGuiKeyValue<574>|ImGuiKeyValue<575>|ImGuiKeyValue<576>|ImGuiKeyValue<577>|ImGuiKeyValue<578>|ImGuiKeyValue<579>|ImGuiKeyValue<580>|ImGuiKeyValue<581>|ImGuiKeyValue<582>|ImGuiKeyValue<583>|ImGuiKeyValue<584>|ImGuiKeyValue<585>|ImGuiKeyValue<586>|ImGuiKeyValue<587>|ImGuiKeyValue<588>|ImGuiKeyValue<589>|ImGuiKeyValue<590>|ImGuiKeyValue<591>|ImGuiKeyValue<592>|ImGuiKeyValue<593>|ImGuiKeyValue<594>|ImGuiKeyValue<595>|ImGuiKeyValue<596>|ImGuiKeyValue<597>|ImGuiKeyValue<598>|ImGuiKeyValue<599>|ImGuiKeyValue<600>|ImGuiKeyValue<601>|ImGuiKeyValue<602>|ImGuiKeyValue<603>|ImGuiKeyValue<604>|ImGuiKeyValue<605>|ImGuiKeyValue<606>|ImGuiKeyValue<607>|ImGuiKeyValue<608>|ImGuiKeyValue<609>|ImGuiKeyValue<610>|ImGuiKeyValue<611>|ImGuiKeyValue<612>|ImGuiKeyValue<613>|ImGuiKeyValue<614>|ImGuiKeyValue<615>|ImGuiKeyValue<616>|ImGuiKeyValue<617>|ImGuiKeyValue<618>|ImGuiKeyValue<619>|ImGuiKeyValue<620>|ImGuiKeyValue<621>|ImGuiKeyValue<622>|ImGuiKeyValue<623>|ImGuiKeyValue<624>|ImGuiKeyValue<625>|ImGuiKeyValue<626>|ImGuiKeyValue<627>|ImGuiKeyValue<628>|ImGuiKeyValue<629>|ImGuiKeyValue<630>|ImGuiKeyValue<631>|ImGuiKeyValue<632>|ImGuiKeyValue<633>|ImGuiKeyValue<634>|ImGuiKeyValue<635>|ImGuiKeyValue<636>|ImGuiKeyValue<637>|ImGuiKeyValue<638>|ImGuiKeyValue<639>|ImGuiKeyValue<640>|ImGuiKeyValue<641>|ImGuiKeyValue<642>|ImGuiKeyValue<643>|ImGuiKeyValue<644>|ImGuiKeyValue<645>|ImGuiKeyValue<646>|ImGuiKeyValue<647>|ImGuiKeyValue<648>|ImGuiKeyValue<649>|ImGuiKeyValue<650>|ImGuiKeyValue<651>|ImGuiKeyValue<652>|ImGuiKeyValue<653>|ImGuiKeyValue<654>|ImGuiKeyValue<655>|ImGuiKeyValue<656>|ImGuiKeyValue<657>|ImGuiKeyValue<658>|ImGuiKeyValue<659>|ImGuiKeyValue<660>|ImGuiKeyValue<661>|ImGuiKeyValue<4096>|ImGuiKeyValue<8192>|ImGuiKeyValue<16384>|ImGuiKeyValue<32768>|ImGuiKeyValue<2048>|ImGuiKeyValue<63488>;

export interface ImGuiNavInputValue<T extends number> {
  value: T;
}
export type ImGuiNavInput = ImGuiNavInputValue<0>|ImGuiNavInputValue<1>|ImGuiNavInputValue<2>|ImGuiNavInputValue<3>|ImGuiNavInputValue<4>|ImGuiNavInputValue<5>|ImGuiNavInputValue<6>|ImGuiNavInputValue<7>|ImGuiNavInputValue<8>|ImGuiNavInputValue<9>|ImGuiNavInputValue<10>|ImGuiNavInputValue<11>|ImGuiNavInputValue<12>|ImGuiNavInputValue<13>|ImGuiNavInputValue<14>|ImGuiNavInputValue<15>|ImGuiNavInputValue<16>;

export interface ImGuiConfigFlagsValue<T extends number> {
  value: T;
}
export type ImGuiConfigFlags = ImGuiConfigFlagsValue<0>|ImGuiConfigFlagsValue<1>|ImGuiConfigFlagsValue<2>|ImGuiConfigFlagsValue<4>|ImGuiConfigFlagsValue<8>|ImGuiConfigFlagsValue<16>|ImGuiConfigFlagsValue<32>|ImGuiConfigFlagsValue<1048576>|ImGuiConfigFlagsValue<2097152>;

export interface ImGuiBackendFlagsValue<T extends number> {
  value: T;
}
export type ImGuiBackendFlags = ImGuiBackendFlagsValue<0>|ImGuiBackendFlagsValue<1>|ImGuiBackendFlagsValue<2>|ImGuiBackendFlagsValue<4>|ImGuiBackendFlagsValue<8>;

export interface ImGuiButtonFlagsValue<T extends number> {
  value: T;
}
export type ImGuiButtonFlags = ImGuiButtonFlagsValue<0>|ImGuiButtonFlagsValue<1>|ImGuiButtonFlagsValue<2>|ImGuiButtonFlagsValue<4>|ImGuiButtonFlagsValue<7>|ImGuiButtonFlagsValue<1>;

export interface ImGuiColorEditFlagsValue<T extends number> {
  value: T;
}
export type ImGuiColorEditFlags = ImGuiColorEditFlagsValue<0>|ImGuiColorEditFlagsValue<2>|ImGuiColorEditFlagsValue<4>|ImGuiColorEditFlagsValue<8>|ImGuiColorEditFlagsValue<16>|ImGuiColorEditFlagsValue<32>|ImGuiColorEditFlagsValue<64>|ImGuiColorEditFlagsValue<128>|ImGuiColorEditFlagsValue<256>|ImGuiColorEditFlagsValue<512>|ImGuiColorEditFlagsValue<1024>|ImGuiColorEditFlagsValue<65536>|ImGuiColorEditFlagsValue<131072>|ImGuiColorEditFlagsValue<262144>|ImGuiColorEditFlagsValue<524288>|ImGuiColorEditFlagsValue<1048576>|ImGuiColorEditFlagsValue<2097152>|ImGuiColorEditFlagsValue<4194304>|ImGuiColorEditFlagsValue<8388608>|ImGuiColorEditFlagsValue<16777216>|ImGuiColorEditFlagsValue<33554432>|ImGuiColorEditFlagsValue<67108864>|ImGuiColorEditFlagsValue<134217728>|ImGuiColorEditFlagsValue<268435456>|ImGuiColorEditFlagsValue<177209344>;

export interface ImGuiSliderFlagsValue<T extends number> {
  value: T;
}
export type ImGuiSliderFlags = ImGuiSliderFlagsValue<0>|ImGuiSliderFlagsValue<16>|ImGuiSliderFlagsValue<32>|ImGuiSliderFlagsValue<64>|ImGuiSliderFlagsValue<128>|ImGuiSliderFlagsValue<1879048207>;

export interface ImGuiMouseButtonValue<T extends number> {
  value: T;
}
export type ImGuiMouseButton = ImGuiMouseButtonValue<0>|ImGuiMouseButtonValue<1>|ImGuiMouseButtonValue<2>|ImGuiMouseButtonValue<5>;

export interface ImGuiMouseCursorValue<T extends number> {
  value: T;
}
export type ImGuiMouseCursor = ImGuiMouseCursorValue<-1>|ImGuiMouseCursorValue<0>|ImGuiMouseCursorValue<1>|ImGuiMouseCursorValue<2>|ImGuiMouseCursorValue<3>|ImGuiMouseCursorValue<4>|ImGuiMouseCursorValue<5>|ImGuiMouseCursorValue<6>|ImGuiMouseCursorValue<7>|ImGuiMouseCursorValue<8>|ImGuiMouseCursorValue<9>;

export interface ImGuiMouseSourceValue<T extends number> {
  value: T;
}
export type ImGuiMouseSource = ImGuiMouseSourceValue<0>|ImGuiMouseSourceValue<1>|ImGuiMouseSourceValue<2>|ImGuiMouseSourceValue<3>;

export interface ImGuiCondValue<T extends number> {
  value: T;
}
export type ImGuiCond = ImGuiCondValue<0>|ImGuiCondValue<1>|ImGuiCondValue<2>|ImGuiCondValue<4>|ImGuiCondValue<8>;

export interface ImGuiTableFlagsValue<T extends number> {
  value: T;
}
export type ImGuiTableFlags = ImGuiTableFlagsValue<0>|ImGuiTableFlagsValue<1>|ImGuiTableFlagsValue<2>|ImGuiTableFlagsValue<4>|ImGuiTableFlagsValue<8>|ImGuiTableFlagsValue<16>|ImGuiTableFlagsValue<32>|ImGuiTableFlagsValue<64>|ImGuiTableFlagsValue<128>|ImGuiTableFlagsValue<256>|ImGuiTableFlagsValue<512>|ImGuiTableFlagsValue<1024>|ImGuiTableFlagsValue<384>|ImGuiTableFlagsValue<1536>|ImGuiTableFlagsValue<640>|ImGuiTableFlagsValue<1280>|ImGuiTableFlagsValue<1920>|ImGuiTableFlagsValue<2048>|ImGuiTableFlagsValue<4096>|ImGuiTableFlagsValue<8192>|ImGuiTableFlagsValue<16384>|ImGuiTableFlagsValue<24576>|ImGuiTableFlagsValue<32768>|ImGuiTableFlagsValue<65536>|ImGuiTableFlagsValue<131072>|ImGuiTableFlagsValue<262144>|ImGuiTableFlagsValue<524288>|ImGuiTableFlagsValue<1048576>|ImGuiTableFlagsValue<2097152>|ImGuiTableFlagsValue<4194304>|ImGuiTableFlagsValue<8388608>|ImGuiTableFlagsValue<16777216>|ImGuiTableFlagsValue<33554432>|ImGuiTableFlagsValue<67108864>|ImGuiTableFlagsValue<134217728>|ImGuiTableFlagsValue<268435456>|ImGuiTableFlagsValue<57344>;

export interface ImGuiTableColumnFlagsValue<T extends number> {
  value: T;
}
export type ImGuiTableColumnFlags = ImGuiTableColumnFlagsValue<0>|ImGuiTableColumnFlagsValue<1>|ImGuiTableColumnFlagsValue<2>|ImGuiTableColumnFlagsValue<4>|ImGuiTableColumnFlagsValue<8>|ImGuiTableColumnFlagsValue<16>|ImGuiTableColumnFlagsValue<32>|ImGuiTableColumnFlagsValue<64>|ImGuiTableColumnFlagsValue<128>|ImGuiTableColumnFlagsValue<256>|ImGuiTableColumnFlagsValue<512>|ImGuiTableColumnFlagsValue<1024>|ImGuiTableColumnFlagsValue<2048>|ImGuiTableColumnFlagsValue<4096>|ImGuiTableColumnFlagsValue<8192>|ImGuiTableColumnFlagsValue<16384>|ImGuiTableColumnFlagsValue<32768>|ImGuiTableColumnFlagsValue<65536>|ImGuiTableColumnFlagsValue<131072>|ImGuiTableColumnFlagsValue<262144>|ImGuiTableColumnFlagsValue<16777216>|ImGuiTableColumnFlagsValue<33554432>|ImGuiTableColumnFlagsValue<67108864>|ImGuiTableColumnFlagsValue<134217728>|ImGuiTableColumnFlagsValue<24>|ImGuiTableColumnFlagsValue<196608>|ImGuiTableColumnFlagsValue<251658240>|ImGuiTableColumnFlagsValue<1073741824>;

export interface ImGuiTableRowFlagsValue<T extends number> {
  value: T;
}
export type ImGuiTableRowFlags = ImGuiTableRowFlagsValue<0>|ImGuiTableRowFlagsValue<1>;

export interface ImGuiTableBgTargetValue<T extends number> {
  value: T;
}
export type ImGuiTableBgTarget = ImGuiTableBgTargetValue<0>|ImGuiTableBgTargetValue<1>|ImGuiTableBgTargetValue<2>|ImGuiTableBgTargetValue<3>;

export interface ImDrawFlagsValue<T extends number> {
  value: T;
}
export type ImDrawFlags = ImDrawFlagsValue<0>|ImDrawFlagsValue<1>|ImDrawFlagsValue<16>|ImDrawFlagsValue<32>|ImDrawFlagsValue<64>|ImDrawFlagsValue<128>|ImDrawFlagsValue<256>|ImDrawFlagsValue<48>|ImDrawFlagsValue<192>|ImDrawFlagsValue<80>|ImDrawFlagsValue<160>|ImDrawFlagsValue<240>|ImDrawFlagsValue<240>|ImDrawFlagsValue<496>;

export interface ImDrawListFlagsValue<T extends number> {
  value: T;
}
export type ImDrawListFlags = ImDrawListFlagsValue<0>|ImDrawListFlagsValue<1>|ImDrawListFlagsValue<2>|ImDrawListFlagsValue<4>|ImDrawListFlagsValue<8>;

export interface ImGuiViewportFlagsValue<T extends number> {
  value: T;
}
export type ImGuiViewportFlags = ImGuiViewportFlagsValue<0>|ImGuiViewportFlagsValue<1>|ImGuiViewportFlagsValue<2>|ImGuiViewportFlagsValue<4>;

interface EmbindModule {
  exit(): void;
  resizeWindow(_0: number, _1: number): void;
  setElement(_0: EmbindString): void;
  patchElement(_0: number, _1: EmbindString): void;
  elementInternalOp(_0: number, _1: EmbindString): void;
  setChildren(_0: number, _1: EmbindString): void;
  appendChild(_0: number, _1: number): void;
  getChildren(_0: number): string;
  appendTextToClippedMultiLineTextRenderer(_0: number, _1: EmbindString): void;
  getStyle(): string;
  patchStyle(_0: EmbindString): void;
  setDebug(_0: boolean): void;
  showDebugWindow(): void;
  ImGuiWindowFlags: {None: ImGuiWindowFlagsValue<0>, NoTitleBar: ImGuiWindowFlagsValue<1>, NoResize: ImGuiWindowFlagsValue<2>, NoMove: ImGuiWindowFlagsValue<4>, NoScrollbar: ImGuiWindowFlagsValue<8>, NoScrollWithMouse: ImGuiWindowFlagsValue<16>, NoCollapse: ImGuiWindowFlagsValue<32>, AlwaysAutoResize: ImGuiWindowFlagsValue<64>, NoBackground: ImGuiWindowFlagsValue<128>, NoSavedSettings: ImGuiWindowFlagsValue<256>, NoMouseInputs: ImGuiWindowFlagsValue<512>, MenuBar: ImGuiWindowFlagsValue<1024>, HorizontalScrollbar: ImGuiWindowFlagsValue<2048>, NoFocusOnAppearing: ImGuiWindowFlagsValue<4096>, NoBringToFrontOnFocus: ImGuiWindowFlagsValue<8192>, AlwaysVerticalScrollbar: ImGuiWindowFlagsValue<16384>, AlwaysHorizontalScrollbar: ImGuiWindowFlagsValue<32768>, NoNavInputs: ImGuiWindowFlagsValue<65536>, NoNavFocus: ImGuiWindowFlagsValue<131072>, UnsavedDocument: ImGuiWindowFlagsValue<262144>, NoNav: ImGuiWindowFlagsValue<196608>, NoDecoration: ImGuiWindowFlagsValue<43>, NoInputs: ImGuiWindowFlagsValue<197120>, AlwaysUseWindowPadding: ImGuiWindowFlagsValue<1073741824>};
  ImGuiChildFlags: {None: ImGuiChildFlagsValue<0>, Border: ImGuiChildFlagsValue<1>, AlwaysUseWindowPadding: ImGuiChildFlagsValue<2>, ResizeX: ImGuiChildFlagsValue<4>, ResizeY: ImGuiChildFlagsValue<8>, AutoResizeX: ImGuiChildFlagsValue<16>, AutoResizeY: ImGuiChildFlagsValue<32>, AlwaysAutoResize: ImGuiChildFlagsValue<64>, FrameStyle: ImGuiChildFlagsValue<128>};
  ImGuiInputTextFlags: {None: ImGuiInputTextFlagsValue<0>, CharsDecimal: ImGuiInputTextFlagsValue<1>, CharsHexadecimal: ImGuiInputTextFlagsValue<2>, CharsUppercase: ImGuiInputTextFlagsValue<4>, CharsNoBlank: ImGuiInputTextFlagsValue<8>, AutoSelectAll: ImGuiInputTextFlagsValue<16>, EnterReturnsTrue: ImGuiInputTextFlagsValue<32>, CallbackCompletion: ImGuiInputTextFlagsValue<64>, CallbackHistory: ImGuiInputTextFlagsValue<128>, CallbackAlways: ImGuiInputTextFlagsValue<256>, CallbackCharFilter: ImGuiInputTextFlagsValue<512>, AllowTabInput: ImGuiInputTextFlagsValue<1024>, CtrlEnterForNewLine: ImGuiInputTextFlagsValue<2048>, NoHorizontalScroll: ImGuiInputTextFlagsValue<4096>, AlwaysOverwrite: ImGuiInputTextFlagsValue<8192>, ReadOnly: ImGuiInputTextFlagsValue<16384>, Password: ImGuiInputTextFlagsValue<32768>, NoUndoRedo: ImGuiInputTextFlagsValue<65536>, CharsScientific: ImGuiInputTextFlagsValue<131072>, CallbackResize: ImGuiInputTextFlagsValue<262144>, CallbackEdit: ImGuiInputTextFlagsValue<524288>, EscapeClearsAll: ImGuiInputTextFlagsValue<1048576>};
  ImGuiTreeNodeFlags: {None: ImGuiTreeNodeFlagsValue<0>, Selected: ImGuiTreeNodeFlagsValue<1>, Framed: ImGuiTreeNodeFlagsValue<2>, AllowOverlap: ImGuiTreeNodeFlagsValue<4>, NoTreePushOnOpen: ImGuiTreeNodeFlagsValue<8>, NoAutoOpenOnLog: ImGuiTreeNodeFlagsValue<16>, DefaultOpen: ImGuiTreeNodeFlagsValue<32>, OpenOnDoubleClick: ImGuiTreeNodeFlagsValue<64>, OpenOnArrow: ImGuiTreeNodeFlagsValue<128>, Leaf: ImGuiTreeNodeFlagsValue<256>, Bullet: ImGuiTreeNodeFlagsValue<512>, FramePadding: ImGuiTreeNodeFlagsValue<1024>, SpanAvailWidth: ImGuiTreeNodeFlagsValue<2048>, SpanFullWidth: ImGuiTreeNodeFlagsValue<4096>, SpanTextWidth: ImGuiTreeNodeFlagsValue<8192>, SpanAllColumns: ImGuiTreeNodeFlagsValue<16384>, NavLeftJumpsBackHere: ImGuiTreeNodeFlagsValue<32768>, CollapsingHeader: ImGuiTreeNodeFlagsValue<26>, AllowItemOverlap: ImGuiTreeNodeFlagsValue<4>};
  ImGuiPopupFlags: {None: ImGuiPopupFlagsValue<0>, MouseButtonLeft: ImGuiPopupFlagsValue<0>, MouseButtonRight: ImGuiPopupFlagsValue<1>, MouseButtonMiddle: ImGuiPopupFlagsValue<2>, MouseButtonMask_: ImGuiPopupFlagsValue<31>, MouseButtonDefault_: ImGuiPopupFlagsValue<1>, NoReopen: ImGuiPopupFlagsValue<32>, NoOpenOverExistingPopup: ImGuiPopupFlagsValue<128>, NoOpenOverItems: ImGuiPopupFlagsValue<256>, AnyPopupId: ImGuiPopupFlagsValue<1024>, AnyPopupLevel: ImGuiPopupFlagsValue<2048>, AnyPopup: ImGuiPopupFlagsValue<3072>};
  ImGuiSelectableFlags: {None: ImGuiSelectableFlagsValue<0>, DontClosePopups: ImGuiSelectableFlagsValue<1>, SpanAllColumns: ImGuiSelectableFlagsValue<2>, AllowDoubleClick: ImGuiSelectableFlagsValue<4>, Disabled: ImGuiSelectableFlagsValue<8>, AllowOverlap: ImGuiSelectableFlagsValue<16>, ImGuiSelectableFlags_AllowItemOverlap: ImGuiSelectableFlagsValue<16>};
  ImGuiComboFlags: {None: ImGuiComboFlagsValue<0>, PopupAlignLeft: ImGuiComboFlagsValue<1>, HeightSmall: ImGuiComboFlagsValue<2>, HeightRegular: ImGuiComboFlagsValue<4>, HeightLarge: ImGuiComboFlagsValue<8>, HeightLargest: ImGuiComboFlagsValue<16>, NoArrowButton: ImGuiComboFlagsValue<32>, NoPreview: ImGuiComboFlagsValue<64>, WidthFitPreview: ImGuiComboFlagsValue<128>, HeightMask_: ImGuiComboFlagsValue<30>};
  ImGuiTabBarFlags: {None: ImGuiTabBarFlagsValue<0>, Reorderable: ImGuiTabBarFlagsValue<1>, AutoSelectNewTabs: ImGuiTabBarFlagsValue<2>, TabListPopupButton: ImGuiTabBarFlagsValue<4>, NoCloseWithMiddleMouseButton: ImGuiTabBarFlagsValue<8>, NoTabListScrollingButtons: ImGuiTabBarFlagsValue<16>, NoTooltip: ImGuiTabBarFlagsValue<32>, FittingPolicyResizeDown: ImGuiTabBarFlagsValue<64>, FittingPolicyScroll: ImGuiTabBarFlagsValue<128>, FittingPolicyMask_: ImGuiTabBarFlagsValue<192>, FittingPolicyDefault_: ImGuiTabBarFlagsValue<64>};
  ImGuiTabItemFlags: {None: ImGuiTabItemFlagsValue<0>, UnsavedDocument: ImGuiTabItemFlagsValue<1>, SetSelected: ImGuiTabItemFlagsValue<2>, NoCloseWithMiddleMouseButton: ImGuiTabItemFlagsValue<4>, NoPushId: ImGuiTabItemFlagsValue<8>, NoTooltip: ImGuiTabItemFlagsValue<16>, NoReorder: ImGuiTabItemFlagsValue<32>, Leading: ImGuiTabItemFlagsValue<64>, Trailing: ImGuiTabItemFlagsValue<128>, NoAssumedClosure: ImGuiTabItemFlagsValue<256>};
  ImGuiFocusedFlags: {None: ImGuiFocusedFlagsValue<0>, ChildWindows: ImGuiFocusedFlagsValue<1>, RootWindow: ImGuiFocusedFlagsValue<2>, AnyWindow: ImGuiFocusedFlagsValue<4>, NoPopupHierarchy: ImGuiFocusedFlagsValue<8>, RootAndChildWindows: ImGuiFocusedFlagsValue<3>};
  ImGuiHoveredFlags: {None: ImGuiHoveredFlagsValue<0>, ChildWindows: ImGuiHoveredFlagsValue<1>, RootWindow: ImGuiHoveredFlagsValue<2>, AnyWindow: ImGuiHoveredFlagsValue<4>, NoPopupHierarchy: ImGuiHoveredFlagsValue<8>, AllowWhenBlockedByPopup: ImGuiHoveredFlagsValue<32>, AllowWhenBlockedByActiveItem: ImGuiHoveredFlagsValue<128>, AllowWhenOverlappedByItem: ImGuiHoveredFlagsValue<256>, AllowWhenOverlappedByWindow: ImGuiHoveredFlagsValue<512>, AllowWhenDisabled: ImGuiHoveredFlagsValue<1024>, NoNavOverride: ImGuiHoveredFlagsValue<2048>, AllowWhenOverlapped: ImGuiHoveredFlagsValue<768>, RectOnly: ImGuiHoveredFlagsValue<928>, RootAndChildWindows: ImGuiHoveredFlagsValue<3>, ForTooltip: ImGuiHoveredFlagsValue<4096>, Stationary: ImGuiHoveredFlagsValue<8192>, DelayNone: ImGuiHoveredFlagsValue<16384>, DelayShort: ImGuiHoveredFlagsValue<32768>, DelayNormal: ImGuiHoveredFlagsValue<65536>, NoSharedDelay: ImGuiHoveredFlagsValue<131072>};
  ImGuiDragDropFlags: {None: ImGuiDragDropFlagsValue<0>, SourceNoPreviewTooltip: ImGuiDragDropFlagsValue<1>, SourceNoDisableHover: ImGuiDragDropFlagsValue<2>, SourceNoHoldToOpenOthers: ImGuiDragDropFlagsValue<4>, SourceAllowNullID: ImGuiDragDropFlagsValue<8>, SourceExtern: ImGuiDragDropFlagsValue<16>, SourceAutoExpirePayload: ImGuiDragDropFlagsValue<32>, AcceptBeforeDelivery: ImGuiDragDropFlagsValue<1024>, AcceptNoDrawDefaultRect: ImGuiDragDropFlagsValue<2048>, AcceptNoPreviewTooltip: ImGuiDragDropFlagsValue<4096>, AcceptPeekOnly: ImGuiDragDropFlagsValue<3072>};
  ImGuiDataType: {S8: ImGuiDataTypeValue<0>, U8: ImGuiDataTypeValue<1>, S16: ImGuiDataTypeValue<2>, U16: ImGuiDataTypeValue<3>, S32: ImGuiDataTypeValue<4>, U32: ImGuiDataTypeValue<5>, S64: ImGuiDataTypeValue<6>, U64: ImGuiDataTypeValue<7>, Float: ImGuiDataTypeValue<8>, Double: ImGuiDataTypeValue<9>, COUNT: ImGuiDataTypeValue<10>};
  ImGuiDir: {None: ImGuiDirValue<-1>, Left: ImGuiDirValue<0>, Right: ImGuiDirValue<1>, Up: ImGuiDirValue<2>, Down: ImGuiDirValue<3>, COUNT: ImGuiDirValue<4>};
  ImGuiSortDirection: {None: ImGuiSortDirectionValue<0>, Ascending: ImGuiSortDirectionValue<1>, Descending: ImGuiSortDirectionValue<2>};
  ImGuiKey: {None: ImGuiKeyValue<0>, Tab: ImGuiKeyValue<512>, LeftArrow: ImGuiKeyValue<513>, RightArrow: ImGuiKeyValue<514>, UpArrow: ImGuiKeyValue<515>, DownArrow: ImGuiKeyValue<516>, PageUp: ImGuiKeyValue<517>, PageDown: ImGuiKeyValue<518>, Home: ImGuiKeyValue<519>, End: ImGuiKeyValue<520>, Insert: ImGuiKeyValue<521>, Delete: ImGuiKeyValue<522>, Backspace: ImGuiKeyValue<523>, Space: ImGuiKeyValue<524>, Enter: ImGuiKeyValue<525>, Escape: ImGuiKeyValue<526>, LeftCtrl: ImGuiKeyValue<527>, LeftShift: ImGuiKeyValue<528>, LeftAlt: ImGuiKeyValue<529>, LeftSuper: ImGuiKeyValue<530>, RightCtrl: ImGuiKeyValue<531>, RightShift: ImGuiKeyValue<532>, RightAlt: ImGuiKeyValue<533>, RightSuper: ImGuiKeyValue<534>, Menu: ImGuiKeyValue<535>, 0: ImGuiKeyValue<536>, 1: ImGuiKeyValue<537>, 2: ImGuiKeyValue<538>, 3: ImGuiKeyValue<539>, 4: ImGuiKeyValue<540>, 5: ImGuiKeyValue<541>, 6: ImGuiKeyValue<542>, 7: ImGuiKeyValue<543>, 8: ImGuiKeyValue<544>, 9: ImGuiKeyValue<545>, A: ImGuiKeyValue<546>, B: ImGuiKeyValue<547>, C: ImGuiKeyValue<548>, D: ImGuiKeyValue<549>, E: ImGuiKeyValue<550>, F: ImGuiKeyValue<551>, G: ImGuiKeyValue<552>, H: ImGuiKeyValue<553>, I: ImGuiKeyValue<554>, J: ImGuiKeyValue<555>, K: ImGuiKeyValue<556>, L: ImGuiKeyValue<557>, M: ImGuiKeyValue<558>, N: ImGuiKeyValue<559>, O: ImGuiKeyValue<560>, P: ImGuiKeyValue<561>, Q: ImGuiKeyValue<562>, R: ImGuiKeyValue<563>, S: ImGuiKeyValue<564>, T: ImGuiKeyValue<565>, U: ImGuiKeyValue<566>, V: ImGuiKeyValue<567>, W: ImGuiKeyValue<568>, X: ImGuiKeyValue<569>, Y: ImGuiKeyValue<570>, Z: ImGuiKeyValue<571>, F1: ImGuiKeyValue<572>, F2: ImGuiKeyValue<573>, F3: ImGuiKeyValue<574>, F4: ImGuiKeyValue<575>, F5: ImGuiKeyValue<576>, F6: ImGuiKeyValue<577>, F7: ImGuiKeyValue<578>, F8: ImGuiKeyValue<579>, F9: ImGuiKeyValue<580>, F10: ImGuiKeyValue<581>, F11: ImGuiKeyValue<582>, F12: ImGuiKeyValue<583>, F13: ImGuiKeyValue<584>, F14: ImGuiKeyValue<585>, F15: ImGuiKeyValue<586>, F16: ImGuiKeyValue<587>, F17: ImGuiKeyValue<588>, F18: ImGuiKeyValue<589>, F19: ImGuiKeyValue<590>, F20: ImGuiKeyValue<591>, F21: ImGuiKeyValue<592>, F22: ImGuiKeyValue<593>, F23: ImGuiKeyValue<594>, F24: ImGuiKeyValue<595>, Apostrophe: ImGuiKeyValue<596>, Comma: ImGuiKeyValue<597>, Minus: ImGuiKeyValue<598>, Period: ImGuiKeyValue<599>, Slash: ImGuiKeyValue<600>, Semicolon: ImGuiKeyValue<601>, Equal: ImGuiKeyValue<602>, LeftBracket: ImGuiKeyValue<603>, Backslash: ImGuiKeyValue<604>, RightBracket: ImGuiKeyValue<605>, GraveAccent: ImGuiKeyValue<606>, CapsLock: ImGuiKeyValue<607>, ScrollLock: ImGuiKeyValue<608>, NumLock: ImGuiKeyValue<609>, PrintScreen: ImGuiKeyValue<610>, Pause: ImGuiKeyValue<611>, Keypad0: ImGuiKeyValue<612>, Keypad1: ImGuiKeyValue<613>, Keypad2: ImGuiKeyValue<614>, Keypad3: ImGuiKeyValue<615>, Keypad4: ImGuiKeyValue<616>, Keypad5: ImGuiKeyValue<617>, Keypad6: ImGuiKeyValue<618>, Keypad7: ImGuiKeyValue<619>, Keypad8: ImGuiKeyValue<620>, Keypad9: ImGuiKeyValue<621>, KeypadDecimal: ImGuiKeyValue<622>, KeypadDivide: ImGuiKeyValue<623>, KeypadMultiply: ImGuiKeyValue<624>, KeypadSubtract: ImGuiKeyValue<625>, KeypadAdd: ImGuiKeyValue<626>, KeypadEnter: ImGuiKeyValue<627>, KeypadEqual: ImGuiKeyValue<628>, AppBack: ImGuiKeyValue<629>, AppForward: ImGuiKeyValue<630>, GamepadStart: ImGuiKeyValue<631>, GamepadBack: ImGuiKeyValue<632>, GamepadFaceLeft: ImGuiKeyValue<633>, GamepadFaceRight: ImGuiKeyValue<634>, GamepadFaceUp: ImGuiKeyValue<635>, GamepadFaceDown: ImGuiKeyValue<636>, GamepadDpadLeft: ImGuiKeyValue<637>, GamepadDpadRight: ImGuiKeyValue<638>, GamepadDpadUp: ImGuiKeyValue<639>, GamepadDpadDown: ImGuiKeyValue<640>, GamepadL1: ImGuiKeyValue<641>, GamepadR1: ImGuiKeyValue<642>, GamepadL2: ImGuiKeyValue<643>, GamepadR2: ImGuiKeyValue<644>, GamepadL3: ImGuiKeyValue<645>, GamepadR3: ImGuiKeyValue<646>, GamepadLStickLeft: ImGuiKeyValue<647>, GamepadLStickRight: ImGuiKeyValue<648>, GamepadLStickUp: ImGuiKeyValue<649>, GamepadLStickDown: ImGuiKeyValue<650>, GamepadRStickLeft: ImGuiKeyValue<651>, GamepadRStickRight: ImGuiKeyValue<652>, GamepadRStickUp: ImGuiKeyValue<653>, GamepadRStickDown: ImGuiKeyValue<654>, MouseLeft: ImGuiKeyValue<655>, MouseRight: ImGuiKeyValue<656>, MouseMiddle: ImGuiKeyValue<657>, MouseX1: ImGuiKeyValue<658>, MouseX2: ImGuiKeyValue<659>, MouseWheelX: ImGuiKeyValue<660>, MouseWheelY: ImGuiKeyValue<661>, Ctrl: ImGuiKeyValue<4096>, Shift: ImGuiKeyValue<8192>, Alt: ImGuiKeyValue<16384>, Super: ImGuiKeyValue<32768>, Shortcut: ImGuiKeyValue<2048>, Mask_: ImGuiKeyValue<63488>};
  ImGuiNavInput: {Activate: ImGuiNavInputValue<0>, Cancel: ImGuiNavInputValue<1>, Input: ImGuiNavInputValue<2>, Menu: ImGuiNavInputValue<3>, DpadLeft: ImGuiNavInputValue<4>, DpadRight: ImGuiNavInputValue<5>, DpadUp: ImGuiNavInputValue<6>, DpadDown: ImGuiNavInputValue<7>, LStickLeft: ImGuiNavInputValue<8>, LStickRight: ImGuiNavInputValue<9>, LStickUp: ImGuiNavInputValue<10>, LStickDown: ImGuiNavInputValue<11>, FocusPrev: ImGuiNavInputValue<12>, FocusNext: ImGuiNavInputValue<13>, TweakSlow: ImGuiNavInputValue<14>, TweakFast: ImGuiNavInputValue<15>, COUNT: ImGuiNavInputValue<16>};
  ImGuiConfigFlags: {None: ImGuiConfigFlagsValue<0>, NavEnableKeyboard: ImGuiConfigFlagsValue<1>, NavEnableGamepad: ImGuiConfigFlagsValue<2>, NavEnableSetMousePos: ImGuiConfigFlagsValue<4>, NavNoCaptureKeyboard: ImGuiConfigFlagsValue<8>, NoMouse: ImGuiConfigFlagsValue<16>, NoMouseCursorChange: ImGuiConfigFlagsValue<32>, IsSRGB: ImGuiConfigFlagsValue<1048576>, IsTouchScreen: ImGuiConfigFlagsValue<2097152>};
  ImGuiBackendFlags: {None: ImGuiBackendFlagsValue<0>, HasGamepad: ImGuiBackendFlagsValue<1>, HasMouseCursors: ImGuiBackendFlagsValue<2>, HasSetMousePos: ImGuiBackendFlagsValue<4>, RendererHasVtxOffset: ImGuiBackendFlagsValue<8>};
  ImGuiButtonFlags: {None: ImGuiButtonFlagsValue<0>, MouseButtonLeft: ImGuiButtonFlagsValue<1>, MouseButtonRight: ImGuiButtonFlagsValue<2>, MouseButtonMiddle: ImGuiButtonFlagsValue<4>, MouseButtonMask_: ImGuiButtonFlagsValue<7>, MouseButtonDefault_: ImGuiButtonFlagsValue<1>};
  ImGuiColorEditFlags: {None: ImGuiColorEditFlagsValue<0>, NoAlpha: ImGuiColorEditFlagsValue<2>, NoPicker: ImGuiColorEditFlagsValue<4>, NoOptions: ImGuiColorEditFlagsValue<8>, NoSmallPreview: ImGuiColorEditFlagsValue<16>, NoInputs: ImGuiColorEditFlagsValue<32>, NoTooltip: ImGuiColorEditFlagsValue<64>, NoLabel: ImGuiColorEditFlagsValue<128>, NoSidePreview: ImGuiColorEditFlagsValue<256>, NoDragDrop: ImGuiColorEditFlagsValue<512>, NoBorder: ImGuiColorEditFlagsValue<1024>, AlphaBar: ImGuiColorEditFlagsValue<65536>, AlphaPreview: ImGuiColorEditFlagsValue<131072>, AlphaPreviewHalf: ImGuiColorEditFlagsValue<262144>, HDR: ImGuiColorEditFlagsValue<524288>, DisplayRGB: ImGuiColorEditFlagsValue<1048576>, DisplayHSV: ImGuiColorEditFlagsValue<2097152>, DisplayHex: ImGuiColorEditFlagsValue<4194304>, Uint8: ImGuiColorEditFlagsValue<8388608>, Float: ImGuiColorEditFlagsValue<16777216>, PickerHueBar: ImGuiColorEditFlagsValue<33554432>, PickerHueWheel: ImGuiColorEditFlagsValue<67108864>, InputRGB: ImGuiColorEditFlagsValue<134217728>, InputHSV: ImGuiColorEditFlagsValue<268435456>, DefaultOptions_: ImGuiColorEditFlagsValue<177209344>};
  ImGuiSliderFlags: {None: ImGuiSliderFlagsValue<0>, AlwaysClamp: ImGuiSliderFlagsValue<16>, Logarithmic: ImGuiSliderFlagsValue<32>, NoRoundToFormat: ImGuiSliderFlagsValue<64>, NoInput: ImGuiSliderFlagsValue<128>, InvalidMask_: ImGuiSliderFlagsValue<1879048207>};
  ImGuiMouseButton: {Left: ImGuiMouseButtonValue<0>, Right: ImGuiMouseButtonValue<1>, Middle: ImGuiMouseButtonValue<2>, COUNT: ImGuiMouseButtonValue<5>};
  ImGuiMouseCursor: {None: ImGuiMouseCursorValue<-1>, Arrow: ImGuiMouseCursorValue<0>, TextInput: ImGuiMouseCursorValue<1>, ResizeAll: ImGuiMouseCursorValue<2>, ResizeNS: ImGuiMouseCursorValue<3>, ResizeEW: ImGuiMouseCursorValue<4>, ResizeNESW: ImGuiMouseCursorValue<5>, ResizeNWSE: ImGuiMouseCursorValue<6>, Hand: ImGuiMouseCursorValue<7>, NotAllowed: ImGuiMouseCursorValue<8>, COUNT: ImGuiMouseCursorValue<9>};
  ImGuiMouseSource: {Mouse: ImGuiMouseSourceValue<0>, TouchScreen: ImGuiMouseSourceValue<1>, Pen: ImGuiMouseSourceValue<2>, COUNT: ImGuiMouseSourceValue<3>};
  ImGuiCond: {None: ImGuiCondValue<0>, Always: ImGuiCondValue<1>, Once: ImGuiCondValue<2>, FirstUseEver: ImGuiCondValue<4>, Appearing: ImGuiCondValue<8>};
  ImGuiTableFlags: {None: ImGuiTableFlagsValue<0>, Resizable: ImGuiTableFlagsValue<1>, Reorderable: ImGuiTableFlagsValue<2>, Hideable: ImGuiTableFlagsValue<4>, Sortable: ImGuiTableFlagsValue<8>, NoSavedSettings: ImGuiTableFlagsValue<16>, ContextMenuInBody: ImGuiTableFlagsValue<32>, RowBg: ImGuiTableFlagsValue<64>, BordersInnerH: ImGuiTableFlagsValue<128>, BordersOuterH: ImGuiTableFlagsValue<256>, BordersInnerV: ImGuiTableFlagsValue<512>, BordersOuterV: ImGuiTableFlagsValue<1024>, BordersH: ImGuiTableFlagsValue<384>, BordersV: ImGuiTableFlagsValue<1536>, BordersInner: ImGuiTableFlagsValue<640>, BordersOuter: ImGuiTableFlagsValue<1280>, Borders: ImGuiTableFlagsValue<1920>, NoBordersInBody: ImGuiTableFlagsValue<2048>, NoBordersInBodyUntilResize: ImGuiTableFlagsValue<4096>, SizingFixedFit: ImGuiTableFlagsValue<8192>, SizingFixedSame: ImGuiTableFlagsValue<16384>, SizingStretchProp: ImGuiTableFlagsValue<24576>, SizingStretchSame: ImGuiTableFlagsValue<32768>, NoHostExtendX: ImGuiTableFlagsValue<65536>, NoHostExtendY: ImGuiTableFlagsValue<131072>, NoKeepColumnsVisible: ImGuiTableFlagsValue<262144>, PreciseWidths: ImGuiTableFlagsValue<524288>, NoClip: ImGuiTableFlagsValue<1048576>, PadOuterX: ImGuiTableFlagsValue<2097152>, NoPadOuterX: ImGuiTableFlagsValue<4194304>, NoPadInnerX: ImGuiTableFlagsValue<8388608>, ScrollX: ImGuiTableFlagsValue<16777216>, ScrollY: ImGuiTableFlagsValue<33554432>, SortMulti: ImGuiTableFlagsValue<67108864>, SortTristate: ImGuiTableFlagsValue<134217728>, HighlightHoveredColumn: ImGuiTableFlagsValue<268435456>, SizingMask_: ImGuiTableFlagsValue<57344>};
  ImGuiTableColumnFlags: {None: ImGuiTableColumnFlagsValue<0>, Disabled: ImGuiTableColumnFlagsValue<1>, DefaultHide: ImGuiTableColumnFlagsValue<2>, DefaultSort: ImGuiTableColumnFlagsValue<4>, WidthStretch: ImGuiTableColumnFlagsValue<8>, WidthFixed: ImGuiTableColumnFlagsValue<16>, NoResize: ImGuiTableColumnFlagsValue<32>, NoReorder: ImGuiTableColumnFlagsValue<64>, NoHide: ImGuiTableColumnFlagsValue<128>, NoClip: ImGuiTableColumnFlagsValue<256>, NoSort: ImGuiTableColumnFlagsValue<512>, NoSortAscending: ImGuiTableColumnFlagsValue<1024>, NoSortDescending: ImGuiTableColumnFlagsValue<2048>, NoHeaderLabel: ImGuiTableColumnFlagsValue<4096>, NoHeaderWidth: ImGuiTableColumnFlagsValue<8192>, PreferSortAscending: ImGuiTableColumnFlagsValue<16384>, PreferSortDescending: ImGuiTableColumnFlagsValue<32768>, IndentEnable: ImGuiTableColumnFlagsValue<65536>, IndentDisable: ImGuiTableColumnFlagsValue<131072>, AngledHeader: ImGuiTableColumnFlagsValue<262144>, IsEnabled: ImGuiTableColumnFlagsValue<16777216>, IsVisible: ImGuiTableColumnFlagsValue<33554432>, IsSorted: ImGuiTableColumnFlagsValue<67108864>, IsHovered: ImGuiTableColumnFlagsValue<134217728>, WidthMask_: ImGuiTableColumnFlagsValue<24>, IndentMask_: ImGuiTableColumnFlagsValue<196608>, StatusMask_: ImGuiTableColumnFlagsValue<251658240>, NoDirectResize_: ImGuiTableColumnFlagsValue<1073741824>};
  ImGuiTableRowFlags: {None: ImGuiTableRowFlagsValue<0>, Headers: ImGuiTableRowFlagsValue<1>};
  ImGuiTableBgTarget: {None: ImGuiTableBgTargetValue<0>, RowBg0: ImGuiTableBgTargetValue<1>, RowBg1: ImGuiTableBgTargetValue<2>, CellBg: ImGuiTableBgTargetValue<3>};
  ImDrawFlags: {None: ImDrawFlagsValue<0>, Closed: ImDrawFlagsValue<1>, RoundCornersTopLeft: ImDrawFlagsValue<16>, RoundCornersTopRight: ImDrawFlagsValue<32>, RoundCornersBottomLeft: ImDrawFlagsValue<64>, RoundCornersBottomRight: ImDrawFlagsValue<128>, RoundCornersNone: ImDrawFlagsValue<256>, RoundCornersTop: ImDrawFlagsValue<48>, RoundCornersBottom: ImDrawFlagsValue<192>, RoundCornersLeft: ImDrawFlagsValue<80>, RoundCornersRight: ImDrawFlagsValue<160>, RoundCornersAll: ImDrawFlagsValue<240>, RoundCornersDefault_: ImDrawFlagsValue<240>, RoundCornersMask_: ImDrawFlagsValue<496>};
  ImDrawListFlags: {None: ImDrawListFlagsValue<0>, AntiAliasedLines: ImDrawListFlagsValue<1>, AntiAliasedLinesUseTex: ImDrawListFlagsValue<2>, AntiAliasedFill: ImDrawListFlagsValue<4>, AllowVtxOffset: ImDrawListFlagsValue<8>};
  ImGuiViewportFlags: {None: ImGuiViewportFlagsValue<0>, IsPlatformWindow: ImGuiViewportFlagsValue<1>, IsPlatformMonitor: ImGuiViewportFlagsValue<2>, OwnedByApp: ImGuiViewportFlagsValue<4>};
}

export type MainModule = WasmModule & typeof RuntimeExports & EmbindModule;
export default function MainModuleFactory (options?: unknown): Promise<MainModule>;
