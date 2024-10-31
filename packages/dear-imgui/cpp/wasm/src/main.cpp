#include <set>
#include <optional>
#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"
#include <emscripten.h>
#include <emscripten/bind.h>
#include <nlohmann/json.hpp>

#include "color_helpers.h"
#include "xframes.h"
#include "implot_renderer.h"

using json = nlohmann::json;

EMSCRIPTEN_DECLARE_VAL_TYPE(OnInitType);
EMSCRIPTEN_DECLARE_VAL_TYPE(OnInputTextChangeType);
EMSCRIPTEN_DECLARE_VAL_TYPE(OnComboChangeType);
EMSCRIPTEN_DECLARE_VAL_TYPE(OnNumericValueChangeType);
EMSCRIPTEN_DECLARE_VAL_TYPE(OnMultiValueChangeType);
EMSCRIPTEN_DECLARE_VAL_TYPE(OnBooleanValueChangeType);
EMSCRIPTEN_DECLARE_VAL_TYPE(OnClickType);

template <typename T>
std::vector<T> JsonToVector(std::string& data) {
    auto parsedData = json::parse(data);
    std::vector<T> vec;
    for (auto& [key, item] : parsedData.items()) {
        vec.push_back(item.template get<T>());
    }
    return vec;
}

template <typename T>
std::set<T> JsonToSet(std::string& data) {
    auto parsedData = json::parse(data);
    std::set<T> set;
    for (auto& [key, item] : parsedData.items()) {
        set.insert(item.template get<T>());
    }
    return set;
}

json IntVectorToJson(const std::vector<int>& data) {
    auto jsonArray = json::array();
    for (auto& item : data) {
        jsonArray.push_back(item);
    }
    return jsonArray;
}

json IntSetToJson(const std::set<int>& data) {
    auto jsonArray = json::array();
    for (auto& item : data) {
        jsonArray.push_back(item);
    }
    return jsonArray;
}

class WasmRunner {
    protected:
        ReactImgui* m_reactImgui{};
        ImGuiRenderer* m_renderer{};

    public:
    WasmRunner() = default;

        static void OnInit() {
            EM_ASM(
                { Module.eventHandlers.onInit(); }
            );
        }

        static void OnTextChanged(const int id, const std::string& value) {
            EM_ASM_ARGS(
                { Module.eventHandlers.onTextChange($0, UTF8ToString($1)); },
                id,
                value.c_str()
            );
        }

        static void OnComboChanged(const int id, const int value) {
            EM_ASM_ARGS(
                { Module.eventHandlers.onComboChange($0, $1); },
                id,
                value
            );
        }

        static void OnNumericValueChanged(const int id, const float value) {
            EM_ASM_ARGS(
                { Module.eventHandlers.onNumericValueChange($0, $1); },
                id,
                value
            );
        }

        static void OnBooleanValueChanged(const int id, const bool value) {
            EM_ASM_ARGS(
                { Module.eventHandlers.onBooleanValueChange($0, $1); },
                id,
                value
            );
        }

        // todo: improve
        static void OnMultipleNumericValuesChanged(const int id, const float* values, const int numValues) {
            if (numValues == 2) {
                EM_ASM_ARGS(
                    {
                        Module.eventHandlers.onMultiValueChange($0, [getValue($1+0, 'float'), getValue($1+4, 'float')]);
                    },
                    id,
                    values
                );
            } else if (numValues == 3) {
                EM_ASM_ARGS(
                    {
                        Module.eventHandlers.onMultiValueChange($0, [getValue($1+0, 'float'), getValue($1+4, 'float'), getValue($1+8, 'float')]);
                    },
                    id,
                    values
                );
            } else if (numValues == 4) {
                EM_ASM_ARGS(
                    {
                        Module.eventHandlers.onMultiValueChange($0, [getValue($1+0, 'float'), getValue($1+4, 'float'), getValue($1+8, 'float'), getValue($1+12, 'float')]);
                    },
                    id,
                    values
                );
            }
        }

        static void OnClick(int const id) {
            EM_ASM_ARGS(
                { Module.eventHandlers.onClick($0); },
                id
            );
        }

        void run(std::string& canvasSelector, std::string& rawFontDefs, std::optional<std::string>& rawStyleOverridesDefs) {
            m_reactImgui = new ReactImgui("XFrames", rawStyleOverridesDefs);
            m_renderer = new ImPlotRenderer(
                m_reactImgui,
                "XFrames",
                "XFrames",
                rawFontDefs,
                std::nullopt
            );
            m_reactImgui->SetEventHandlers(
                OnInit,
                OnTextChanged,
                OnComboChanged,
                OnNumericValueChanged,
                OnMultipleNumericValuesChanged,
                OnBooleanValueChanged,
                OnClick);
            m_renderer->Init(canvasSelector);
        }

        void exit() const {
            emscripten_cancel_main_loop();
            emscripten_force_exit(0);
        }

        void resizeWindow(const int width, const int height) const {
            m_renderer->SetWindowSize(width, height);
        }

        void setElement(std::string& elementJsonAsString) const {
            m_reactImgui->QueueCreateElement(elementJsonAsString);
        }

        void patchElement(const int id, std::string& elementJsonAsString) const {
            m_reactImgui->QueuePatchElement(id, elementJsonAsString);
        }

        void elementInternalOp(const int id, std::string& elementJsonAsString) const {
            m_reactImgui->QueueElementInternalOp(id, elementJsonAsString);
        }

        void setChildren(const int id, const std::vector<int>& childrenIds) const {
            m_reactImgui->QueueSetChildren(id, childrenIds);
        }

        void appendChild(const int parentId, const int childId) const {
            m_reactImgui->QueueAppendChild(parentId, childId);
        }

        [[nodiscard]] std::vector<int> getChildren(const int id) const {
            return m_reactImgui->GetChildren(id);
        }

        [[nodiscard]] std::string getAvailableFonts() const {
            return m_renderer->GetAvailableFonts().dump();
        }

        void appendTextToClippedMultiLineTextRenderer(const int id, const std::string& data) const {
            m_reactImgui->AppendTextToClippedMultiLineTextRenderer(id, data);
        }

        [[nodiscard]] std::string getStyle() const {
            json style;

            style["alpha"] = m_reactImgui->m_appStyle.Alpha;
            style["disabledAlpha"] = m_reactImgui->m_appStyle.DisabledAlpha;
            style["windowPadding"] = { m_reactImgui->m_appStyle.WindowPadding.x, m_reactImgui->m_appStyle.WindowPadding.y };
            style["windowRounding"] = m_reactImgui->m_appStyle.WindowRounding;
            style["windowBorderSize"] = m_reactImgui->m_appStyle.WindowBorderSize;
            style["windowMinSize"] = { m_reactImgui->m_appStyle.WindowMinSize.x, m_reactImgui->m_appStyle.WindowMinSize.y };
            style["windowTitleAlign"] = { m_reactImgui->m_appStyle.WindowTitleAlign.x, m_reactImgui->m_appStyle.WindowTitleAlign.y };
            style["windowMenuButtonPosition"] = m_reactImgui->m_appStyle.WindowMenuButtonPosition;
            style["childRounding"] = m_reactImgui->m_appStyle.ChildRounding;
            style["childBorderSize"] = m_reactImgui->m_appStyle.ChildBorderSize;
            style["popupRounding"] = m_reactImgui->m_appStyle.PopupRounding;
            style["popupBorderSize"] = m_reactImgui->m_appStyle.PopupBorderSize;
            style["framePadding"] = { m_reactImgui->m_appStyle.FramePadding.x, m_reactImgui->m_appStyle.FramePadding.y };
            style["frameRounding"] = m_reactImgui->m_appStyle.FrameRounding;
            style["frameBorderSize"] = m_reactImgui->m_appStyle.FrameBorderSize;
            style["itemSpacing"] = { m_reactImgui->m_appStyle.ItemSpacing.x, m_reactImgui->m_appStyle.ItemSpacing.y };
            style["itemInnerSpacing"] = { m_reactImgui->m_appStyle.ItemInnerSpacing.x, m_reactImgui->m_appStyle.ItemInnerSpacing.y };
            style["cellPadding"] = { m_reactImgui->m_appStyle.CellPadding.x, m_reactImgui->m_appStyle.CellPadding.y };
            style["touchExtraPadding"] = { m_reactImgui->m_appStyle.TouchExtraPadding.x, m_reactImgui->m_appStyle.TouchExtraPadding.y };
            style["indentSpacing"] = m_reactImgui->m_appStyle.IndentSpacing;
            style["columnsMinSpacing"] = m_reactImgui->m_appStyle.ColumnsMinSpacing;
            style["scrollbarSize"] = m_reactImgui->m_appStyle.ScrollbarSize;
            style["scrollbarRounding"] = m_reactImgui->m_appStyle.ScrollbarRounding;
            style["grabMinSize"] = m_reactImgui->m_appStyle.GrabMinSize;
            style["grabRounding"] = m_reactImgui->m_appStyle.GrabRounding;
            style["logSliderDeadzone"] = m_reactImgui->m_appStyle.LogSliderDeadzone;
            style["tabRounding"] = m_reactImgui->m_appStyle.TabRounding;
            style["tabBorderSize"] = m_reactImgui->m_appStyle.TabBorderSize;
            style["tabMinWidthForCloseButton"] = m_reactImgui->m_appStyle.TabMinWidthForCloseButton;
            style["tabBarBorderSize"] = m_reactImgui->m_appStyle.TabBarBorderSize;
            style["tableAngledHeadersAngle"] = m_reactImgui->m_appStyle.TableAngledHeadersAngle;
            style["tableAngledHeadersTextAlign"] = { m_reactImgui->m_appStyle.TableAngledHeadersTextAlign.x, m_reactImgui->m_appStyle.TableAngledHeadersTextAlign.y };
            style["colorButtonPosition"] = m_reactImgui->m_appStyle.ColorButtonPosition;
            style["buttonTextAlign"] = { m_reactImgui->m_appStyle.ButtonTextAlign.x, m_reactImgui->m_appStyle.ButtonTextAlign.y };
            style["selectableTextAlign"] = { m_reactImgui->m_appStyle.SelectableTextAlign.x, m_reactImgui->m_appStyle.SelectableTextAlign.y };
            style["separatorTextPadding"] = { m_reactImgui->m_appStyle.SeparatorTextPadding.x, m_reactImgui->m_appStyle.SeparatorTextPadding.y };
            style["displayWindowPadding"] = { m_reactImgui->m_appStyle.DisplayWindowPadding.x, m_reactImgui->m_appStyle.DisplayWindowPadding.y };
            style["displaySafeAreaPadding"] = { m_reactImgui->m_appStyle.DisplaySafeAreaPadding.x, m_reactImgui->m_appStyle.DisplaySafeAreaPadding.y };
            style["mouseCursorScale"] = m_reactImgui->m_appStyle.MouseCursorScale;
            style["antiAliasedLines"] = m_reactImgui->m_appStyle.AntiAliasedLines;
            style["antiAliasedLinesUseTex"] = m_reactImgui->m_appStyle.AntiAliasedLinesUseTex;
            style["antiAliasedFill"] = m_reactImgui->m_appStyle.AntiAliasedFill;
            style["curveTessellationTol"] = m_reactImgui->m_appStyle.CurveTessellationTol;
            style["circleTessellationMaxError"] = m_reactImgui->m_appStyle.CircleTessellationMaxError;

            style["hoverStationaryDelay"] = m_reactImgui->m_appStyle.HoverStationaryDelay;
            style["hoverDelayShort"] = m_reactImgui->m_appStyle.HoverDelayShort;
            style["hoverDelayNormal"] = m_reactImgui->m_appStyle.HoverDelayNormal;

            style["hoverFlagsForTooltipMouse"] = m_reactImgui->m_appStyle.HoverFlagsForTooltipMouse;
            style["hoverFlagsForTooltipNav"] = m_reactImgui->m_appStyle.HoverFlagsForTooltipNav;

            style["colors"] = json::array();

            for (int i = 0; i < ImGuiCol_COUNT; i++) {
                auto maybeValue = IV4toJsonHEXATuple(m_reactImgui->m_appStyle.Colors[i]);

                if (maybeValue.has_value()) {
                    style["colors"].push_back(maybeValue.value());
                }
            }

            return style.dump();
        }

        void patchStyle(std::string& styleDef) const {
            m_reactImgui->PatchStyle(json::parse(styleDef));
        }

        void setDebug(const bool debug) const {
            m_reactImgui->SetDebug(debug);
        }

        void showDebugWindow() const {
            m_reactImgui->ShowDebugWindow();
        }
};

static std::unique_ptr<WasmRunner> pRunner = std::make_unique<WasmRunner>();

// todo: add validation of arguments
int main(int argc, char* argv[]) {
    std::string canvasSelector = argv[1];
    std::string rawFontDefs = argv[2];
    std::optional<std::string> rawStyleOverridesDefs;

    if (argc > 2) {
        // third argument is style overrides
        rawStyleOverridesDefs.emplace(std::string(argv[3]));
    }

    pRunner->run(canvasSelector, rawFontDefs, rawStyleOverridesDefs);

    return 0;
}

void _exit() {
    pRunner->exit();
}

void resizeWindow(const int width, const int height) {
    pRunner->resizeWindow(width, height);
}

// emscripten::bind cannot receive `elementJson` by reference
void setElement(std::string elementJson) {
    pRunner->setElement(elementJson);
}

void patchElement(const int id, std::string elementJson) {
    pRunner->patchElement(id, elementJson);
}

// emscripten::bind cannot receive `elementJson` by reference
void elementInternalOp(const int id, std::string elementJson) {
    pRunner->elementInternalOp(id, elementJson);
}

// emscripten::bind cannot receive `childrenIds` by reference
void setChildren(const int id, std::string childrenIds) {
    pRunner->setChildren(id, JsonToVector<int>(childrenIds));
}

void appendChild(const int parentId, const int childId) {
    pRunner->appendChild(parentId, childId);
}

std::string getChildren(const int id) {
    return IntVectorToJson(pRunner->getChildren(id)).dump();
}

// emscripten::bind cannot receive `data` by reference
void appendTextToClippedMultiLineTextRenderer(const int id, std::string data) {
    pRunner->appendTextToClippedMultiLineTextRenderer(id, data);
}

std::string getStyle() {
    return pRunner->getStyle();
}

// emscripten::bind cannot receive `styleDef` by reference
void patchStyle(std::string styleDef) {
    return pRunner->patchStyle(styleDef);
}

void setDebug(const bool debug) {
    return pRunner->setDebug(debug);
}

void showDebugWindow() {
    pRunner->showDebugWindow();
}

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("exit", &_exit);
    emscripten::function("resizeWindow", &resizeWindow);
    emscripten::function("setElement", &setElement);
    emscripten::function("patchElement", &patchElement);
    emscripten::function("elementInternalOp", &elementInternalOp);
    emscripten::function("setChildren", &setChildren);
    emscripten::function("appendChild", &appendChild);
    emscripten::function("getChildren", &getChildren);
    emscripten::function("appendTextToClippedMultiLineTextRenderer", &appendTextToClippedMultiLineTextRenderer);
    emscripten::function("getStyle", &getStyle);
    emscripten::function("patchStyle", &patchStyle);
    emscripten::function("setDebug", &setDebug);
    emscripten::function("showDebugWindow", &showDebugWindow);

    // emscripten::class_<WasmRunner>("WasmRunner")
    // .constructor<OnInputTextChangeType, OnComboChangeType, OnNumericValueChangeType, OnMultiValueChangeType, OnBooleanValueChangeType, OnClickType>()
    // .function("run", &WasmRunner::run)
    // .function("exit", &WasmRunner::exit)
    // .function("resizeWindow", &WasmRunner::resizeWindow)
    // .function("setWidget", &WasmRunner::setWidget)
    // .function("patchWidget", &WasmRunner::patchWidget)
    // .function("setChildren", &WasmRunner::setChildren)
    // .function("getChildren", &WasmRunner::getChildren)
    // .function("getAvailableFonts", &WasmRunner::getAvailableFonts)
    // .function("appendChartData", &WasmRunner::appendChartData)
    // .function("setAxesDecimalPlaces", &WasmRunner::setAxesDecimalPlaces)
    // .function("resetChartData", &WasmRunner::resetChartData)
    ;

    // emscripten::register_vector<int>("IntVector");

    emscripten::register_type<OnInitType>("() => void");
    emscripten::register_type<OnInputTextChangeType>("(id: string, value: string) => void");
    emscripten::register_type<OnComboChangeType>("(id: string, value: number) => void");
    emscripten::register_type<OnNumericValueChangeType>("(id: string, value: number) => void");
    emscripten::register_type<OnMultiValueChangeType>("(id: string, values: Primitive[]) => void");
    emscripten::register_type<OnBooleanValueChangeType>("(id: string, value: boolean) => void");
    emscripten::register_type<OnClickType>("(id: string) => void");

    emscripten::enum_<ImGuiWindowFlags_>("ImGuiWindowFlags")
        .value("None", ImGuiWindowFlags_None)
        .value("NoTitleBar", ImGuiWindowFlags_NoTitleBar)
        .value("NoResize", ImGuiWindowFlags_NoResize)
        .value("NoMove", ImGuiWindowFlags_NoMove)
        .value("NoScrollbar", ImGuiWindowFlags_NoScrollbar)
        .value("NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse)
        .value("NoCollapse", ImGuiWindowFlags_NoCollapse)
        .value("AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize)
        .value("NoBackground", ImGuiWindowFlags_NoBackground)
        .value("NoSavedSettings", ImGuiWindowFlags_NoSavedSettings)
        .value("NoMouseInputs", ImGuiWindowFlags_NoMouseInputs)
        .value("MenuBar", ImGuiWindowFlags_MenuBar)
        .value("HorizontalScrollbar", ImGuiWindowFlags_HorizontalScrollbar)
        .value("NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing)
        .value("NoBringToFrontOnFocus", ImGuiWindowFlags_NoBringToFrontOnFocus)
        .value("AlwaysVerticalScrollbar", ImGuiWindowFlags_AlwaysVerticalScrollbar)
        .value("AlwaysHorizontalScrollbar", ImGuiWindowFlags_AlwaysHorizontalScrollbar)
        .value("NoNavInputs", ImGuiWindowFlags_NoNavInputs)
        .value("NoNavFocus", ImGuiWindowFlags_NoNavFocus)
        .value("UnsavedDocument", ImGuiWindowFlags_UnsavedDocument)
        .value("NoNav", ImGuiWindowFlags_NoNav)
        .value("NoDecoration", ImGuiWindowFlags_NoDecoration)
        .value("NoInputs", ImGuiWindowFlags_NoInputs)
        // Deprecated
        .value("AlwaysUseWindowPadding", ImGuiWindowFlags_AlwaysUseWindowPadding)
        ;

    emscripten::enum_<ImGuiChildFlags_>("ImGuiChildFlags")
        .value("None", ImGuiChildFlags_None)
        .value("Border", ImGuiChildFlags_Border)
        .value("AlwaysUseWindowPadding", ImGuiChildFlags_AlwaysUseWindowPadding)
        .value("ResizeX", ImGuiChildFlags_ResizeX)
        .value("ResizeY", ImGuiChildFlags_ResizeY)
        .value("AutoResizeX", ImGuiChildFlags_AutoResizeX)
        .value("AutoResizeY", ImGuiChildFlags_AutoResizeY)
        .value("AlwaysAutoResize", ImGuiChildFlags_AlwaysAutoResize)
        .value("FrameStyle", ImGuiChildFlags_FrameStyle)
        ;

    emscripten::enum_<ImGuiInputTextFlags_>("ImGuiInputTextFlags")
        .value("None", ImGuiInputTextFlags_None)
        .value("CharsDecimal", ImGuiInputTextFlags_CharsDecimal)
        .value("CharsHexadecimal", ImGuiInputTextFlags_CharsHexadecimal)
        .value("CharsUppercase", ImGuiInputTextFlags_CharsUppercase)
        .value("CharsNoBlank", ImGuiInputTextFlags_CharsNoBlank)
        .value("AutoSelectAll", ImGuiInputTextFlags_AutoSelectAll)
        .value("EnterReturnsTrue", ImGuiInputTextFlags_EnterReturnsTrue)
        .value("CallbackCompletion", ImGuiInputTextFlags_CallbackCompletion)
        .value("CallbackHistory", ImGuiInputTextFlags_CallbackHistory)
        .value("CallbackAlways", ImGuiInputTextFlags_CallbackAlways)
        .value("CallbackCharFilter", ImGuiInputTextFlags_CallbackCharFilter)
        .value("AllowTabInput", ImGuiInputTextFlags_AllowTabInput)
        .value("CtrlEnterForNewLine", ImGuiInputTextFlags_CtrlEnterForNewLine)
        .value("NoHorizontalScroll", ImGuiInputTextFlags_NoHorizontalScroll)
        .value("AlwaysOverwrite", ImGuiInputTextFlags_AlwaysOverwrite)
        .value("ReadOnly", ImGuiInputTextFlags_ReadOnly)
        .value("Password", ImGuiInputTextFlags_Password)
        .value("NoUndoRedo", ImGuiInputTextFlags_NoUndoRedo)
        .value("CharsScientific", ImGuiInputTextFlags_CharsScientific)
        .value("CallbackResize", ImGuiInputTextFlags_CallbackResize)
        .value("CallbackEdit", ImGuiInputTextFlags_CallbackEdit)
        .value("EscapeClearsAll", ImGuiInputTextFlags_EscapeClearsAll)
        ;

    emscripten::enum_<ImGuiTreeNodeFlags_>("ImGuiTreeNodeFlags")
        .value("None", ImGuiTreeNodeFlags_None)
        .value("Selected", ImGuiTreeNodeFlags_Selected)
        .value("Framed", ImGuiTreeNodeFlags_Framed)
        .value("AllowOverlap", ImGuiTreeNodeFlags_AllowOverlap)
        .value("NoTreePushOnOpen", ImGuiTreeNodeFlags_NoTreePushOnOpen)
        .value("NoAutoOpenOnLog", ImGuiTreeNodeFlags_NoAutoOpenOnLog)
        .value("DefaultOpen", ImGuiTreeNodeFlags_DefaultOpen)
        .value("OpenOnDoubleClick", ImGuiTreeNodeFlags_OpenOnDoubleClick)
        .value("OpenOnArrow", ImGuiTreeNodeFlags_OpenOnArrow)
        .value("Leaf", ImGuiTreeNodeFlags_Leaf)
        .value("Bullet", ImGuiTreeNodeFlags_Bullet)
        .value("FramePadding", ImGuiTreeNodeFlags_FramePadding)
        .value("SpanAvailWidth", ImGuiTreeNodeFlags_SpanAvailWidth)
        .value("SpanFullWidth", ImGuiTreeNodeFlags_SpanFullWidth)
        .value("SpanTextWidth", ImGuiTreeNodeFlags_SpanTextWidth)
        .value("SpanAllColumns", ImGuiTreeNodeFlags_SpanAllColumns)
        .value("NavLeftJumpsBackHere", ImGuiTreeNodeFlags_NavLeftJumpsBackHere)
        // .value("ImGuiTreeNodeFlags_NoScrollOnOpen", ImGuiTreeNodeFlags_NoScrollOnOpen) WIP
        .value("CollapsingHeader", ImGuiTreeNodeFlags_CollapsingHeader)
        // Renamed in 1.89.7
        .value("AllowItemOverlap", ImGuiTreeNodeFlags_AllowItemOverlap)
        ;

    emscripten::enum_<ImGuiPopupFlags_>("ImGuiPopupFlags")
        .value("None", ImGuiPopupFlags_None)
        .value("MouseButtonLeft", ImGuiPopupFlags_MouseButtonLeft)
        .value("MouseButtonRight", ImGuiPopupFlags_MouseButtonRight)
        .value("MouseButtonMiddle", ImGuiPopupFlags_MouseButtonMiddle)
        .value("MouseButtonMask_", ImGuiPopupFlags_MouseButtonMask_)
        .value("MouseButtonDefault_", ImGuiPopupFlags_MouseButtonDefault_)
        .value("NoReopen", ImGuiPopupFlags_NoReopen)
        // .value("NoReopenAlwaysNavInit", ImGuiPopupFlags_NoReopenAlwaysNavInit)
        .value("NoOpenOverExistingPopup", ImGuiPopupFlags_NoOpenOverExistingPopup)
        .value("NoOpenOverItems", ImGuiPopupFlags_NoOpenOverItems)
        .value("AnyPopupId", ImGuiPopupFlags_AnyPopupId)
        .value("AnyPopupLevel", ImGuiPopupFlags_AnyPopupLevel)
        .value("AnyPopup", ImGuiPopupFlags_AnyPopup)
        ;

    emscripten::enum_<ImGuiSelectableFlags_>("ImGuiSelectableFlags")
        .value("None", ImGuiSelectableFlags_None)
        .value("DontClosePopups", ImGuiSelectableFlags_DontClosePopups)
        .value("SpanAllColumns", ImGuiSelectableFlags_SpanAllColumns)
        .value("AllowDoubleClick", ImGuiSelectableFlags_AllowDoubleClick)
        .value("Disabled", ImGuiSelectableFlags_Disabled)
        .value("AllowOverlap", ImGuiSelectableFlags_AllowOverlap)
        // Deprecated
        .value("ImGuiSelectableFlags_AllowItemOverlap", ImGuiSelectableFlags_AllowItemOverlap)
        ;

    emscripten::enum_<ImGuiComboFlags_>("ImGuiComboFlags")
        .value("None", ImGuiComboFlags_None)
        .value("PopupAlignLeft", ImGuiComboFlags_PopupAlignLeft)
        .value("HeightSmall", ImGuiComboFlags_HeightSmall)
        .value("HeightRegular", ImGuiComboFlags_HeightRegular)
        .value("HeightLarge", ImGuiComboFlags_HeightLarge)
        .value("HeightLargest", ImGuiComboFlags_HeightLargest)
        .value("NoArrowButton", ImGuiComboFlags_NoArrowButton)
        .value("NoPreview", ImGuiComboFlags_NoPreview)
        .value("WidthFitPreview", ImGuiComboFlags_WidthFitPreview)
        .value("HeightMask_", ImGuiComboFlags_HeightMask_)
        ;

    emscripten::enum_<ImGuiTabBarFlags_>("ImGuiTabBarFlags")
        .value("None", ImGuiTabBarFlags_None)
        .value("Reorderable", ImGuiTabBarFlags_Reorderable)
        .value("AutoSelectNewTabs", ImGuiTabBarFlags_AutoSelectNewTabs)
        .value("TabListPopupButton", ImGuiTabBarFlags_TabListPopupButton)
        .value("NoCloseWithMiddleMouseButton", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)
        .value("NoTabListScrollingButtons", ImGuiTabBarFlags_NoTabListScrollingButtons)
        .value("NoTooltip", ImGuiTabBarFlags_NoTooltip)
        .value("FittingPolicyResizeDown", ImGuiTabBarFlags_FittingPolicyResizeDown)
        .value("FittingPolicyScroll", ImGuiTabBarFlags_FittingPolicyScroll)
        .value("FittingPolicyMask_", ImGuiTabBarFlags_FittingPolicyMask_)
        .value("FittingPolicyDefault_", ImGuiTabBarFlags_FittingPolicyDefault_)
        ;

    emscripten::enum_<ImGuiTabItemFlags_>("ImGuiTabItemFlags")
        .value("None", ImGuiTabItemFlags_None)
        .value("UnsavedDocument", ImGuiTabItemFlags_UnsavedDocument)
        .value("SetSelected", ImGuiTabItemFlags_SetSelected)
        .value("NoCloseWithMiddleMouseButton", ImGuiTabItemFlags_NoCloseWithMiddleMouseButton)
        .value("NoPushId", ImGuiTabItemFlags_NoPushId)
        .value("NoTooltip", ImGuiTabItemFlags_NoTooltip)
        .value("NoReorder", ImGuiTabItemFlags_NoReorder)
        .value("Leading", ImGuiTabItemFlags_Leading)
        .value("Trailing", ImGuiTabItemFlags_Trailing)
        .value("NoAssumedClosure", ImGuiTabItemFlags_NoAssumedClosure)
        ;

    emscripten::enum_<ImGuiFocusedFlags_>("ImGuiFocusedFlags")
        .value("None", ImGuiFocusedFlags_None)
        .value("ChildWindows", ImGuiFocusedFlags_ChildWindows)
        .value("RootWindow", ImGuiFocusedFlags_RootWindow)
        .value("AnyWindow", ImGuiFocusedFlags_AnyWindow)
        .value("NoPopupHierarchy", ImGuiFocusedFlags_NoPopupHierarchy)
        // .value("DockHierarchy", ImGuiFocusedFlags_DockHierarchy)
        .value("RootAndChildWindows", ImGuiFocusedFlags_RootAndChildWindows)
        ;

    emscripten::enum_<ImGuiHoveredFlags_>("ImGuiHoveredFlags")
        .value("None", ImGuiHoveredFlags_None)
        .value("ChildWindows", ImGuiHoveredFlags_ChildWindows)
        .value("RootWindow", ImGuiHoveredFlags_RootWindow)
        .value("AnyWindow", ImGuiHoveredFlags_AnyWindow)
        .value("NoPopupHierarchy", ImGuiHoveredFlags_NoPopupHierarchy)
        // .value("DockHierarchy", ImGuiHoveredFlags_DockHierarchy)
        .value("AllowWhenBlockedByPopup", ImGuiHoveredFlags_AllowWhenBlockedByPopup)
        // .value("AllowWhenBlockedByModal", ImGuiHoveredFlags_AllowWhenBlockedByModal)
        .value("AllowWhenBlockedByActiveItem", ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
        .value("AllowWhenOverlappedByItem", ImGuiHoveredFlags_AllowWhenOverlappedByItem)
        .value("AllowWhenOverlappedByWindow", ImGuiHoveredFlags_AllowWhenOverlappedByWindow)
        .value("AllowWhenDisabled", ImGuiHoveredFlags_AllowWhenDisabled)
        .value("NoNavOverride", ImGuiHoveredFlags_NoNavOverride)
        .value("AllowWhenOverlapped", ImGuiHoveredFlags_AllowWhenOverlapped)
        .value("RectOnly", ImGuiHoveredFlags_RectOnly)
        .value("RootAndChildWindows", ImGuiHoveredFlags_RootAndChildWindows)
        .value("ForTooltip", ImGuiHoveredFlags_ForTooltip)
        .value("Stationary", ImGuiHoveredFlags_Stationary)
        .value("DelayNone", ImGuiHoveredFlags_DelayNone)
        .value("DelayShort", ImGuiHoveredFlags_DelayShort)
        .value("DelayNormal", ImGuiHoveredFlags_DelayNormal)
        .value("NoSharedDelay", ImGuiHoveredFlags_NoSharedDelay)
        ;

    emscripten::enum_<ImGuiDragDropFlags_>("ImGuiDragDropFlags")
        .value("None", ImGuiDragDropFlags_None)
        .value("SourceNoPreviewTooltip", ImGuiDragDropFlags_SourceNoPreviewTooltip)
        .value("SourceNoDisableHover", ImGuiDragDropFlags_SourceNoDisableHover)
        .value("SourceNoHoldToOpenOthers", ImGuiDragDropFlags_SourceNoHoldToOpenOthers)
        .value("SourceAllowNullID", ImGuiDragDropFlags_SourceAllowNullID)
        .value("SourceExtern", ImGuiDragDropFlags_SourceExtern)
        .value("SourceAutoExpirePayload", ImGuiDragDropFlags_SourceAutoExpirePayload)
        .value("AcceptBeforeDelivery", ImGuiDragDropFlags_AcceptBeforeDelivery)
        .value("AcceptNoDrawDefaultRect", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)
        .value("AcceptNoPreviewTooltip", ImGuiDragDropFlags_AcceptNoPreviewTooltip)
        .value("AcceptPeekOnly", ImGuiDragDropFlags_AcceptPeekOnly)
        ;

    emscripten::enum_<ImGuiDataType_>("ImGuiDataType")
        .value("S8", ImGuiDataType_S8)
        .value("U8", ImGuiDataType_U8)
        .value("S16", ImGuiDataType_S16)
        .value("U16", ImGuiDataType_U16)
        .value("S32", ImGuiDataType_S32)
        .value("U32", ImGuiDataType_U32)
        .value("S64", ImGuiDataType_S64)
        .value("U64", ImGuiDataType_U64)
        .value("Float", ImGuiDataType_Float)
        .value("Double", ImGuiDataType_Double)
        .value("COUNT", ImGuiDataType_COUNT)
        ;

    emscripten::enum_<ImGuiDir_>("ImGuiDir")
        .value("None", ImGuiDir_None)
        .value("Left", ImGuiDir_Left)
        .value("Right", ImGuiDir_Right)
        .value("Up", ImGuiDir_Up)
        .value("Down", ImGuiDir_Down)
        .value("COUNT", ImGuiDir_COUNT)
        ;

    emscripten::enum_<ImGuiSortDirection_>("ImGuiSortDirection")
        .value("None", ImGuiSortDirection_None)
        .value("Ascending", ImGuiSortDirection_Ascending)
        .value("Descending", ImGuiSortDirection_Descending)
        ;

    emscripten::enum_<ImGuiKey>("ImGuiKey")
        .value("None", ImGuiKey_None)
        .value("Tab", ImGuiKey_Tab)
        .value("LeftArrow", ImGuiKey_LeftArrow)
        .value("RightArrow", ImGuiKey_RightArrow)
        .value("UpArrow", ImGuiKey_UpArrow)
        .value("DownArrow", ImGuiKey_DownArrow)
        .value("PageUp", ImGuiKey_PageUp)
        .value("PageDown", ImGuiKey_PageDown)
        .value("Home", ImGuiKey_Home)
        .value("End", ImGuiKey_End)
        .value("Insert", ImGuiKey_Insert)
        .value("Delete", ImGuiKey_Delete)
        .value("Backspace", ImGuiKey_Backspace)
        .value("Space", ImGuiKey_Space)
        .value("Enter", ImGuiKey_Enter)
        .value("Escape", ImGuiKey_Escape)
        .value("LeftCtrl", ImGuiKey_LeftCtrl)
        .value("LeftShift", ImGuiKey_LeftShift)
        .value("LeftAlt", ImGuiKey_LeftAlt)
        .value("LeftSuper", ImGuiKey_LeftSuper)
        .value("RightCtrl", ImGuiKey_RightCtrl)
        .value("RightShift", ImGuiKey_RightShift)
        .value("RightAlt", ImGuiKey_RightAlt)
        .value("RightSuper", ImGuiKey_RightSuper)
        .value("Menu", ImGuiKey_Menu)
        .value("0", ImGuiKey_0)
        .value("1", ImGuiKey_1)
        .value("2", ImGuiKey_2)
        .value("3", ImGuiKey_3)
        .value("4", ImGuiKey_4)
        .value("5", ImGuiKey_5)
        .value("6", ImGuiKey_6)
        .value("7", ImGuiKey_7)
        .value("8", ImGuiKey_8)
        .value("9", ImGuiKey_9)
        .value("A", ImGuiKey_A)
        .value("B", ImGuiKey_B)
        .value("C", ImGuiKey_C)
        .value("D", ImGuiKey_D)
        .value("E", ImGuiKey_E)
        .value("F", ImGuiKey_F)
        .value("G", ImGuiKey_G)
        .value("H", ImGuiKey_H)
        .value("I", ImGuiKey_I)
        .value("J", ImGuiKey_J)
        .value("K", ImGuiKey_K)
        .value("L", ImGuiKey_L)
        .value("M", ImGuiKey_M)
        .value("N", ImGuiKey_N)
        .value("O", ImGuiKey_O)
        .value("P", ImGuiKey_P)
        .value("Q", ImGuiKey_Q)
        .value("R", ImGuiKey_R)
        .value("S", ImGuiKey_S)
        .value("T", ImGuiKey_T)
        .value("U", ImGuiKey_U)
        .value("V", ImGuiKey_V)
        .value("W", ImGuiKey_W)
        .value("X", ImGuiKey_X)
        .value("Y", ImGuiKey_Y)
        .value("Z", ImGuiKey_Z)
        .value("F1", ImGuiKey_F1)
        .value("F2", ImGuiKey_F2)
        .value("F3", ImGuiKey_F3)
        .value("F4", ImGuiKey_F4)
        .value("F5", ImGuiKey_F5)
        .value("F6", ImGuiKey_F6)
        .value("F7", ImGuiKey_F7)
        .value("F8", ImGuiKey_F8)
        .value("F9", ImGuiKey_F9)
        .value("F10", ImGuiKey_F10)
        .value("F11", ImGuiKey_F11)
        .value("F12", ImGuiKey_F12)
        .value("F13", ImGuiKey_F13)
        .value("F14", ImGuiKey_F14)
        .value("F15", ImGuiKey_F15)
        .value("F16", ImGuiKey_F16)
        .value("F17", ImGuiKey_F17)
        .value("F18", ImGuiKey_F18)
        .value("F19", ImGuiKey_F19)
        .value("F20", ImGuiKey_F20)
        .value("F21", ImGuiKey_F21)
        .value("F22", ImGuiKey_F22)
        .value("F23", ImGuiKey_F23)
        .value("F24", ImGuiKey_F24)
        .value("Apostrophe", ImGuiKey_Apostrophe)
        .value("Comma", ImGuiKey_Comma)
        .value("Minus", ImGuiKey_Minus)
        .value("Period", ImGuiKey_Period)
        .value("Slash", ImGuiKey_Slash)
        .value("Semicolon", ImGuiKey_Semicolon)
        .value("Equal", ImGuiKey_Equal)
        .value("LeftBracket", ImGuiKey_LeftBracket)
        .value("Backslash", ImGuiKey_Backslash)
        .value("RightBracket", ImGuiKey_RightBracket)
        .value("GraveAccent", ImGuiKey_GraveAccent)
        .value("CapsLock", ImGuiKey_CapsLock)
        .value("ScrollLock", ImGuiKey_ScrollLock)
        .value("NumLock", ImGuiKey_NumLock)
        .value("PrintScreen", ImGuiKey_PrintScreen)
        .value("Pause", ImGuiKey_Pause)
        .value("Keypad0", ImGuiKey_Keypad0)
        .value("Keypad1", ImGuiKey_Keypad1)
        .value("Keypad2", ImGuiKey_Keypad2)
        .value("Keypad3", ImGuiKey_Keypad3)
        .value("Keypad4", ImGuiKey_Keypad4)
        .value("Keypad5", ImGuiKey_Keypad5)
        .value("Keypad6", ImGuiKey_Keypad6)
        .value("Keypad7", ImGuiKey_Keypad7)
        .value("Keypad8", ImGuiKey_Keypad8)
        .value("Keypad9", ImGuiKey_Keypad9)
        .value("KeypadDecimal", ImGuiKey_KeypadDecimal)
        .value("KeypadDivide", ImGuiKey_KeypadDivide)
        .value("KeypadMultiply", ImGuiKey_KeypadMultiply)
        .value("KeypadSubtract", ImGuiKey_KeypadSubtract)
        .value("KeypadAdd", ImGuiKey_KeypadAdd)
        .value("KeypadEnter", ImGuiKey_KeypadEnter)
        .value("KeypadEqual", ImGuiKey_KeypadEqual)
        .value("AppBack", ImGuiKey_AppBack)
        .value("AppForward", ImGuiKey_AppForward)
        .value("GamepadStart", ImGuiKey_GamepadStart)
        .value("GamepadBack", ImGuiKey_GamepadBack)
        .value("GamepadFaceLeft", ImGuiKey_GamepadFaceLeft)
        .value("GamepadFaceRight", ImGuiKey_GamepadFaceRight)
        .value("GamepadFaceUp", ImGuiKey_GamepadFaceUp)
        .value("GamepadFaceDown", ImGuiKey_GamepadFaceDown)
        .value("GamepadDpadLeft", ImGuiKey_GamepadDpadLeft)
        .value("GamepadDpadRight", ImGuiKey_GamepadDpadRight)
        .value("GamepadDpadUp", ImGuiKey_GamepadDpadUp)
        .value("GamepadDpadDown", ImGuiKey_GamepadDpadDown)
        .value("GamepadL1", ImGuiKey_GamepadL1)
        .value("GamepadR1", ImGuiKey_GamepadR1)
        .value("GamepadL2", ImGuiKey_GamepadL2)
        .value("GamepadR2", ImGuiKey_GamepadR2)
        .value("GamepadL3", ImGuiKey_GamepadL3)
        .value("GamepadR3", ImGuiKey_GamepadR3)
        .value("GamepadLStickLeft", ImGuiKey_GamepadLStickLeft)
        .value("GamepadLStickRight", ImGuiKey_GamepadLStickRight)
        .value("GamepadLStickUp", ImGuiKey_GamepadLStickUp)
        .value("GamepadLStickDown", ImGuiKey_GamepadLStickDown)
        .value("GamepadRStickLeft", ImGuiKey_GamepadRStickLeft)
        .value("GamepadRStickRight", ImGuiKey_GamepadRStickRight)
        .value("GamepadRStickUp", ImGuiKey_GamepadRStickUp)
        .value("GamepadRStickDown", ImGuiKey_GamepadRStickDown)
        .value("MouseLeft", ImGuiKey_MouseLeft)
        .value("MouseRight", ImGuiKey_MouseRight)
        .value("MouseMiddle", ImGuiKey_MouseMiddle)
        .value("MouseX1", ImGuiKey_MouseX1)
        .value("MouseX2", ImGuiKey_MouseX2)
        .value("MouseWheelX", ImGuiKey_MouseWheelX)
        .value("MouseWheelY", ImGuiKey_MouseWheelY)
        // .value("None", ImGuiMod_None) // Duplicate (ImGuiKey_None)
        .value("Ctrl", ImGuiMod_Ctrl)
        .value("Shift", ImGuiMod_Shift)
        .value("Alt", ImGuiMod_Alt)
        .value("Super", ImGuiMod_Super)
        .value("Shortcut", ImGuiMod_Shortcut)
        .value("Mask_", ImGuiMod_Mask_)
        ;

    emscripten::enum_<ImGuiNavInput>("ImGuiNavInput")
        .value("Activate", ImGuiNavInput_Activate)
        .value("Cancel", ImGuiNavInput_Cancel)
        .value("Input", ImGuiNavInput_Input)
        .value("Menu", ImGuiNavInput_Menu)
        .value("DpadLeft", ImGuiNavInput_DpadLeft)
        .value("DpadRight", ImGuiNavInput_DpadRight)
        .value("DpadUp", ImGuiNavInput_DpadUp)
        .value("DpadDown", ImGuiNavInput_DpadDown)
        .value("LStickLeft", ImGuiNavInput_LStickLeft)
        .value("LStickRight", ImGuiNavInput_LStickRight)
        .value("LStickUp", ImGuiNavInput_LStickUp)
        .value("LStickDown", ImGuiNavInput_LStickDown)
        .value("FocusPrev", ImGuiNavInput_FocusPrev)
        .value("FocusNext", ImGuiNavInput_FocusNext)
        .value("TweakSlow", ImGuiNavInput_TweakSlow)
        .value("TweakFast", ImGuiNavInput_TweakFast)
        .value("COUNT", ImGuiNavInput_COUNT)
        ;

    emscripten::enum_<ImGuiConfigFlags_>("ImGuiConfigFlags")
        .value("None", ImGuiConfigFlags_None)
        .value("NavEnableKeyboard", ImGuiConfigFlags_NavEnableKeyboard)
        .value("NavEnableGamepad", ImGuiConfigFlags_NavEnableGamepad)
        .value("NavEnableSetMousePos", ImGuiConfigFlags_NavEnableSetMousePos)
        .value("NavNoCaptureKeyboard", ImGuiConfigFlags_NavNoCaptureKeyboard)
        .value("NoMouse", ImGuiConfigFlags_NoMouse)
        .value("NoMouseCursorChange", ImGuiConfigFlags_NoMouseCursorChange)
        .value("IsSRGB", ImGuiConfigFlags_IsSRGB)
        .value("IsTouchScreen", ImGuiConfigFlags_IsTouchScreen)
        ;

    emscripten::enum_<ImGuiBackendFlags_>("ImGuiBackendFlags")
        .value("None", ImGuiBackendFlags_None)
        .value("HasGamepad", ImGuiBackendFlags_HasGamepad)
        .value("HasMouseCursors", ImGuiBackendFlags_HasMouseCursors)
        .value("HasSetMousePos", ImGuiBackendFlags_HasSetMousePos)
        .value("RendererHasVtxOffset", ImGuiBackendFlags_RendererHasVtxOffset)
        ;

    emscripten::enum_<ImGuiButtonFlags_>("ImGuiButtonFlags")
        .value("None", ImGuiButtonFlags_None)
        .value("MouseButtonLeft", ImGuiButtonFlags_MouseButtonLeft)
        .value("MouseButtonRight", ImGuiButtonFlags_MouseButtonRight)
        .value("MouseButtonMiddle", ImGuiButtonFlags_MouseButtonMiddle)
        .value("MouseButtonMask_", ImGuiButtonFlags_MouseButtonMask_)
        .value("MouseButtonDefault_", ImGuiButtonFlags_MouseButtonDefault_)
        ;

    emscripten::enum_<ImGuiColorEditFlags_>("ImGuiColorEditFlags")
        .value("None", ImGuiColorEditFlags_None)
        .value("NoAlpha", ImGuiColorEditFlags_NoAlpha)
        .value("NoPicker", ImGuiColorEditFlags_NoPicker)
        .value("NoOptions", ImGuiColorEditFlags_NoOptions)
        .value("NoSmallPreview", ImGuiColorEditFlags_NoSmallPreview)
        .value("NoInputs", ImGuiColorEditFlags_NoInputs)
        .value("NoTooltip", ImGuiColorEditFlags_NoTooltip)
        .value("NoLabel", ImGuiColorEditFlags_NoLabel)
        .value("NoSidePreview", ImGuiColorEditFlags_NoSidePreview)
        .value("NoDragDrop", ImGuiColorEditFlags_NoDragDrop)
        .value("NoBorder", ImGuiColorEditFlags_NoBorder)
        .value("AlphaBar", ImGuiColorEditFlags_AlphaBar)
        .value("AlphaPreview", ImGuiColorEditFlags_AlphaPreview)
        .value("AlphaPreviewHalf", ImGuiColorEditFlags_AlphaPreviewHalf)
        .value("HDR", ImGuiColorEditFlags_HDR)
        .value("DisplayRGB", ImGuiColorEditFlags_DisplayRGB)
        .value("DisplayHSV", ImGuiColorEditFlags_DisplayHSV)
        .value("DisplayHex", ImGuiColorEditFlags_DisplayHex)
        .value("Uint8", ImGuiColorEditFlags_Uint8)
        .value("Float", ImGuiColorEditFlags_Float)
        .value("PickerHueBar", ImGuiColorEditFlags_PickerHueBar)
        .value("PickerHueWheel", ImGuiColorEditFlags_PickerHueWheel)
        .value("InputRGB", ImGuiColorEditFlags_InputRGB)
        .value("InputHSV", ImGuiColorEditFlags_InputHSV)
        .value("DefaultOptions_", ImGuiColorEditFlags_DefaultOptions_)
        ;

    emscripten::enum_<ImGuiSliderFlags_>("ImGuiSliderFlags")
        .value("None", ImGuiSliderFlags_None)
        .value("AlwaysClamp", ImGuiSliderFlags_AlwaysClamp)
        .value("Logarithmic", ImGuiSliderFlags_Logarithmic)
        .value("NoRoundToFormat", ImGuiSliderFlags_NoRoundToFormat)
        .value("NoInput", ImGuiSliderFlags_NoInput)
        .value("InvalidMask_", ImGuiSliderFlags_InvalidMask_)
        ;

    emscripten::enum_<ImGuiMouseButton_>("ImGuiMouseButton")
        .value("Left", ImGuiMouseButton_Left)
        .value("Right", ImGuiMouseButton_Right)
        .value("Middle", ImGuiMouseButton_Middle)
        .value("COUNT", ImGuiMouseButton_COUNT)
        ;

    emscripten::enum_<ImGuiMouseCursor_>("ImGuiMouseCursor")
        .value("None", ImGuiMouseCursor_None)
        .value("Arrow", ImGuiMouseCursor_Arrow)
        .value("TextInput", ImGuiMouseCursor_TextInput)
        .value("ResizeAll", ImGuiMouseCursor_ResizeAll)
        .value("ResizeNS", ImGuiMouseCursor_ResizeNS)
        .value("ResizeEW", ImGuiMouseCursor_ResizeEW)
        .value("ResizeNESW", ImGuiMouseCursor_ResizeNESW)
        .value("ResizeNWSE", ImGuiMouseCursor_ResizeNWSE)
        .value("Hand", ImGuiMouseCursor_Hand)
        .value("NotAllowed", ImGuiMouseCursor_NotAllowed)
        .value("COUNT", ImGuiMouseCursor_COUNT)
        ;

    emscripten::enum_<ImGuiMouseSource>("ImGuiMouseSource")
        .value("Mouse", ImGuiMouseSource_Mouse)
        .value("TouchScreen", ImGuiMouseSource_TouchScreen)
        .value("Pen", ImGuiMouseSource_Pen)
        .value("COUNT", ImGuiMouseSource_COUNT)
        ;

    emscripten::enum_<ImGuiCond_>("ImGuiCond")
        .value("None", ImGuiCond_None)
        .value("Always", ImGuiCond_Always)
        .value("Once", ImGuiCond_Once)
        .value("FirstUseEver", ImGuiCond_FirstUseEver)
        .value("Appearing", ImGuiCond_Appearing)
        ;

    emscripten::enum_<ImGuiTableFlags_>("ImGuiTableFlags")
        .value("None", ImGuiTableFlags_None)
        .value("Resizable", ImGuiTableFlags_Resizable)
        .value("Reorderable", ImGuiTableFlags_Reorderable)
        .value("Hideable", ImGuiTableFlags_Hideable)
        .value("Sortable", ImGuiTableFlags_Sortable)
        .value("NoSavedSettings", ImGuiTableFlags_NoSavedSettings)
        .value("ContextMenuInBody", ImGuiTableFlags_ContextMenuInBody)
        .value("RowBg", ImGuiTableFlags_RowBg)
        .value("BordersInnerH", ImGuiTableFlags_BordersInnerH)
        .value("BordersOuterH", ImGuiTableFlags_BordersOuterH)
        .value("BordersInnerV", ImGuiTableFlags_BordersInnerV)
        .value("BordersOuterV", ImGuiTableFlags_BordersOuterV)
        .value("BordersH", ImGuiTableFlags_BordersH)
        .value("BordersV", ImGuiTableFlags_BordersV)
        .value("BordersInner", ImGuiTableFlags_BordersInner)
        .value("BordersOuter", ImGuiTableFlags_BordersOuter)
        .value("Borders", ImGuiTableFlags_Borders)
        .value("NoBordersInBody", ImGuiTableFlags_NoBordersInBody)
        .value("NoBordersInBodyUntilResize", ImGuiTableFlags_NoBordersInBodyUntilResize)
        .value("SizingFixedFit", ImGuiTableFlags_SizingFixedFit)
        .value("SizingFixedSame", ImGuiTableFlags_SizingFixedSame)
        .value("SizingStretchProp", ImGuiTableFlags_SizingStretchProp)
        .value("SizingStretchSame", ImGuiTableFlags_SizingStretchSame)
        .value("NoHostExtendX", ImGuiTableFlags_NoHostExtendX)
        .value("NoHostExtendY", ImGuiTableFlags_NoHostExtendY)
        .value("NoKeepColumnsVisible", ImGuiTableFlags_NoKeepColumnsVisible)
        .value("PreciseWidths", ImGuiTableFlags_PreciseWidths)
        .value("NoClip", ImGuiTableFlags_NoClip)
        .value("PadOuterX", ImGuiTableFlags_PadOuterX)
        .value("NoPadOuterX", ImGuiTableFlags_NoPadOuterX)
        .value("NoPadInnerX", ImGuiTableFlags_NoPadInnerX)
        .value("ScrollX", ImGuiTableFlags_ScrollX)
        .value("ScrollY", ImGuiTableFlags_ScrollY)
        .value("SortMulti", ImGuiTableFlags_SortMulti)
        .value("SortTristate", ImGuiTableFlags_SortTristate)
        .value("HighlightHoveredColumn", ImGuiTableFlags_HighlightHoveredColumn)
        .value("SizingMask_", ImGuiTableFlags_SizingMask_)
        ;

    emscripten::enum_<ImGuiTableColumnFlags_>("ImGuiTableColumnFlags")
        .value("None", ImGuiTableColumnFlags_None)
        .value("Disabled", ImGuiTableColumnFlags_Disabled)
        .value("DefaultHide", ImGuiTableColumnFlags_DefaultHide)
        .value("DefaultSort", ImGuiTableColumnFlags_DefaultSort)
        .value("WidthStretch", ImGuiTableColumnFlags_WidthStretch)
        .value("WidthFixed", ImGuiTableColumnFlags_WidthFixed)
        .value("NoResize", ImGuiTableColumnFlags_NoResize)
        .value("NoReorder", ImGuiTableColumnFlags_NoReorder)
        .value("NoHide", ImGuiTableColumnFlags_NoHide)
        .value("NoClip", ImGuiTableColumnFlags_NoClip)
        .value("NoSort", ImGuiTableColumnFlags_NoSort)
        .value("NoSortAscending", ImGuiTableColumnFlags_NoSortAscending)
        .value("NoSortDescending", ImGuiTableColumnFlags_NoSortDescending)
        .value("NoHeaderLabel", ImGuiTableColumnFlags_NoHeaderLabel)
        .value("NoHeaderWidth", ImGuiTableColumnFlags_NoHeaderWidth)
        .value("PreferSortAscending", ImGuiTableColumnFlags_PreferSortAscending)
        .value("PreferSortDescending", ImGuiTableColumnFlags_PreferSortDescending)
        .value("IndentEnable", ImGuiTableColumnFlags_IndentEnable)
        .value("IndentDisable", ImGuiTableColumnFlags_IndentDisable)
        .value("AngledHeader", ImGuiTableColumnFlags_AngledHeader)
        .value("IsEnabled", ImGuiTableColumnFlags_IsEnabled)
        .value("IsVisible", ImGuiTableColumnFlags_IsVisible)
        .value("IsSorted", ImGuiTableColumnFlags_IsSorted)
        .value("IsHovered", ImGuiTableColumnFlags_IsHovered)
        .value("WidthMask_", ImGuiTableColumnFlags_WidthMask_)
        .value("IndentMask_", ImGuiTableColumnFlags_IndentMask_)
        .value("StatusMask_", ImGuiTableColumnFlags_StatusMask_)
        .value("NoDirectResize_", ImGuiTableColumnFlags_NoDirectResize_)
        ;

    emscripten::enum_<ImGuiTableRowFlags_>("ImGuiTableRowFlags")
        .value("None", ImGuiTableRowFlags_None)
        .value("Headers", ImGuiTableRowFlags_Headers)
        ;

    emscripten::enum_<ImGuiTableBgTarget_>("ImGuiTableBgTarget")
        .value("None", ImGuiTableBgTarget_None)
        .value("RowBg0", ImGuiTableBgTarget_RowBg0)
        .value("RowBg1", ImGuiTableBgTarget_RowBg1)
        .value("CellBg", ImGuiTableBgTarget_CellBg)
        ;

    emscripten::enum_<ImDrawFlags_>("ImDrawFlags")
        .value("None", ImDrawFlags_None)
        .value("Closed", ImDrawFlags_Closed)
        .value("RoundCornersTopLeft", ImDrawFlags_RoundCornersTopLeft)
        .value("RoundCornersTopRight", ImDrawFlags_RoundCornersTopRight)
        .value("RoundCornersBottomLeft", ImDrawFlags_RoundCornersBottomLeft)
        .value("RoundCornersBottomRight", ImDrawFlags_RoundCornersBottomRight)
        .value("RoundCornersNone", ImDrawFlags_RoundCornersNone)
        .value("RoundCornersTop", ImDrawFlags_RoundCornersTop)
        .value("RoundCornersBottom", ImDrawFlags_RoundCornersBottom)
        .value("RoundCornersLeft", ImDrawFlags_RoundCornersLeft)
        .value("RoundCornersRight", ImDrawFlags_RoundCornersRight)
        .value("RoundCornersAll", ImDrawFlags_RoundCornersAll)
        .value("RoundCornersDefault_", ImDrawFlags_RoundCornersDefault_)
        .value("RoundCornersMask_", ImDrawFlags_RoundCornersMask_)
        ;

    emscripten::enum_<ImDrawListFlags_>("ImDrawListFlags")
        .value("None", ImDrawListFlags_None)
        .value("AntiAliasedLines", ImDrawListFlags_AntiAliasedLines)
        .value("AntiAliasedLinesUseTex", ImDrawListFlags_AntiAliasedLinesUseTex)
        .value("AntiAliasedFill", ImDrawListFlags_AntiAliasedFill)
        .value("AllowVtxOffset", ImDrawListFlags_AllowVtxOffset)
        ;

    emscripten::enum_<ImGuiViewportFlags_>("ImGuiViewportFlags")
        .value("None", ImGuiViewportFlags_None)
        .value("IsPlatformWindow", ImGuiViewportFlags_IsPlatformWindow)
        .value("IsPlatformMonitor", ImGuiViewportFlags_IsPlatformMonitor)
        .value("OwnedByApp", ImGuiViewportFlags_OwnedByApp)
        ;

}
