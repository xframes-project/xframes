export type ImVec2 = [number, number];

export enum ImGuiCol {
    Text,
    TextDisabled,
    WindowBg,
    ChildBg,
    PopupBg,
    Border,
    BorderShadow,
    FrameBg,
    FrameBgHovered,
    FrameBgActive,
    TitleBg,
    TitleBgActive,
    TitleBgCollapsed,
    MenuBarBg,
    ScrollbarBg,
    ScrollbarGrab,
    ScrollbarGrabHovered,
    ScrollbarGrabActive,
    CheckMark,
    SliderGrab,
    SliderGrabActive,
    Button,
    ButtonHovered,
    ButtonActive,
    Header,
    HeaderHovered,
    HeaderActive,
    Separator,
    SeparatorHovered,
    SeparatorActive,
    ResizeGrip,
    ResizeGripHovered,
    ResizeGripActive,
    Tab,
    TabHovered,
    TabActive,
    TabUnfocused,
    TabUnfocusedActive,
    PlotLines,
    PlotLinesHovered,
    PlotHistogram,
    PlotHistogramHovered,
    TableHeaderBg,
    TableBorderStrong,
    TableBorderLight,
    TableRowBg,
    TableRowBgAlt,
    TextSelectedBg,
    DragDropTarget,
    NavHighlight,
    NavWindowingHighlight,
    NavWindowingDimBg,
    ModalWindowDimBg,
    COUNT,
}

export enum ImPlotScale {
    Linear,
    Time,
    Log10,
    SymLog,
}

export enum ImPlotMarker {
    None = -1,
    Circle,
    Square,
    Diamond,
    Up,
    Down,
    Left,
    Right,
    Cross,
    Plus,
    Asterisk,
}

export enum ImGuiStyleVar {
    Alpha, // float     Alpha
    DisabledAlpha, // float     DisabledAlpha
    WindowPadding, // ImVec2    WindowPadding
    WindowRounding, // float     WindowRounding
    WindowBorderSize, // float     WindowBorderSize
    WindowMinSize, // ImVec2    WindowMinSize
    WindowTitleAlign, // ImVec2    WindowTitleAlign
    ChildRounding, // float     ChildRounding
    ChildBorderSize, // float     ChildBorderSize
    PopupRounding, // float     PopupRounding
    PopupBorderSize, // float     PopupBorderSize
    FramePadding, // ImVec2    FramePadding
    FrameRounding, // float     FrameRounding
    FrameBorderSize, // float     FrameBorderSize
    ItemSpacing, // ImVec2    ItemSpacing
    ItemInnerSpacing, // ImVec2    ItemInnerSpacing
    IndentSpacing, // float     IndentSpacing
    CellPadding, // ImVec2    CellPadding
    ScrollbarSize, // float     ScrollbarSize
    ScrollbarRounding, // float     ScrollbarRounding
    GrabMinSize, // float     GrabMinSize
    GrabRounding, // float     GrabRounding
    TabRounding, // float     TabRounding
    TabBorderSize, // float     TabBorderSize
    TabBarBorderSize, // float     TabBarBorderSize
    TableAngledHeadersAngle, // float  TableAngledHeadersAngle
    TableAngledHeadersTextAlign, // ImVec2 TableAngledHeadersTextAlign
    ButtonTextAlign, // ImVec2    ButtonTextAlign
    SelectableTextAlign, // ImVec2    SelectableTextAlign
    SeparatorTextBorderSize, // float  SeparatorTextBorderSize
    SeparatorTextAlign, // ImVec2    SeparatorTextAlign
    SeparatorTextPadding, // ImVec2    SeparatorTextPadding
}

export enum ImGuiDir {
    None = -1,
    Left = 0,
    Right = 1,
    Up = 2,
    Down = 3,
}

export enum ImGuiHoveredFlags {
    None = 0,
    ChildWindows = 1 << 0,
    RootWindow = 1 << 1,
    AnyWindow = 1 << 2,
    NoPopupHierarchy = 1 << 3,
    // DockHierarchy               = 1 << 4,
    AllowWhenBlockedByPopup = 1 << 5,
    // AllowWhenBlockedByModal     = 1 << 6,
    AllowWhenBlockedByActiveItem = 1 << 7,
    AllowWhenOverlappedByItem = 1 << 8,
    AllowWhenOverlappedByWindow = 1 << 9,
    AllowWhenDisabled = 1 << 10,
    NoNavOverride = 1 << 11,
    AllowWhenOverlapped = ImGuiHoveredFlags.AllowWhenOverlappedByItem |
        ImGuiHoveredFlags.AllowWhenOverlappedByWindow,
    RectOnly = ImGuiHoveredFlags.AllowWhenBlockedByPopup |
        ImGuiHoveredFlags.AllowWhenBlockedByActiveItem |
        ImGuiHoveredFlags.AllowWhenOverlapped,
    RootAndChildWindows = ImGuiHoveredFlags.RootWindow | ImGuiHoveredFlags.ChildWindows,

    ForTooltip = 1 << 12,

    Stationary = 1 << 13,
    DelayNone = 1 << 14,
    DelayShort = 1 << 15,
    DelayNormal = 1 << 16,
    NoSharedDelay = 1 << 17,
}
