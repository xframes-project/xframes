#include <cstring>
#include <string>
#include <functional>
#include <sstream>
#include <utility>

#include <GLES3/gl3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "implot.h"
#include "implot_internal.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#include "imgui_impl_wgpu.h"
#else
#include "imgui_impl_opengl3.h"
#endif

#include <rpp/rpp.hpp>
#include <nlohmann/json.hpp>

#include "element/layout_node.h"

#include "shared.h"
#include "implot_renderer.h"
#include "reactimgui.h"

#include "color_helpers.h"
#include "implot_renderer.h"

#ifdef __EMSCRIPTEN__
#include "widget/map_view.h"
#endif

#include "widget/button.h"
#include "widget/checkbox.h"
#include "widget/child.h"
#include "widget/clipped_multi_line_text_renderer.h"
#include "widget/collapsing_header.h"
#include "widget/combo.h"
#include "widget/group.h"
#include "widget/image.h"
#include "widget/input_text.h"
#include "widget/item_tooltip.h"
#include "widget/multi_slider.h"
#include "widget/plot_candlestick.h"
#include "widget/plot_line.h"
#include "widget/separator.h"
#include "widget/separator_text.h"
#include "widget/slider.h"
#include "widget/widget.h"
#include "widget/styled_widget.h"
#include "widget/table.h"
#include "widget/tabs.h"
#include "widget/text.h"
#include "widget/text_wrap.h"
#include "widget/tree_node.h"
#include "widget/window.h"

using json = nlohmann::json;

template <typename T, typename std::enable_if<std::is_base_of<Widget, T>::value, int>::type>
std::unique_ptr<T> makeWidget(const json& val, std::optional<WidgetStyle> maybeStyle, ReactImgui* view) {
    return T::makeWidget(val, maybeStyle, view);
}

std::unique_ptr<Element> makeElement(const json& val, ReactImgui* view) {
    return Element::makeElement(val, view);
}

ReactImgui::ReactImgui(
    const char* windowId,
    std::optional<std::string> rawStyleOverridesDefs
) {
    m_windowId = windowId;
    m_debug = false;
    m_rawStyleOverridesDefs = std::move(rawStyleOverridesDefs);

    SetUpElementCreatorFunctions();
    SetUpFloatFormatChars();
}

void ReactImgui::SetDebug(bool debug) {
    m_debug = debug;

    if (m_debug) {
        ImGui::SetWindowFocus("debug");
    }
};

void ReactImgui::ShowDebugWindow() {
    ImGui::SetWindowFocus("debug");
};

void ReactImgui::SetUpSubjects() {
    auto handler = [this](const ElementOpDef& elementOpDef) {
        switch(elementOpDef.op) {
            case OpCreateElement: {
                CreateElement(elementOpDef.data);
                break;
            }
            case OpPatchElement: {
                PatchElement(elementOpDef.data);
                break;
            }
            case OpSetChildren: {
                SetChildren(elementOpDef.data);
                break;
            }
            case OpAppendChild: {
                AppendChild(elementOpDef.data);
                break;
            }

            default: break;
        }
    };

    m_elementOpSubject = rpp::subjects::serialized_replay_subject<ElementOpDef>{100};
    m_elementOpSubject.get_observable() | rpp::ops::subscribe(handler);
};

void ReactImgui::SetUpElementCreatorFunctions() {
    m_element_init_fn["group"] = &makeWidget<Group>;
    m_element_init_fn["child"] = &makeWidget<Child>;
    m_element_init_fn["di-window"] = &makeWidget<Window>;
    m_element_init_fn["separator"] = &makeWidget<Separator>;

    m_element_init_fn["collapsing-header"] = &makeWidget<CollapsingHeader>;
    m_element_init_fn["tab-bar"] = &makeWidget<TabBar>;
    m_element_init_fn["tab-item"] = &makeWidget<TabItem>;
    m_element_init_fn["tree-node"] = &makeWidget<TreeNode>;

    m_element_init_fn["di-table"] = &makeWidget<Table>;
    m_element_init_fn["clipped-multi-line-text-renderer"] = &makeWidget<ClippedMultiLineTextRenderer>;

    m_element_init_fn["di-image"] = &makeWidget<Image>;

#ifdef __EMSCRIPTEN__
    m_element_init_fn["map-view"] = &makeWidget<MapView>;
#endif

    m_element_init_fn["plot-line"] = &makeWidget<PlotLine>;
    m_element_init_fn["plot-candlestick"] = &makeWidget<PlotCandlestick>;

    m_element_init_fn["item-tooltip"] = &makeWidget<ItemTooltip>;

    m_element_init_fn["combo"] = &makeWidget<Combo>;
    m_element_init_fn["slider"] = &makeWidget<Slider>;
    m_element_init_fn["input-text"] = &makeWidget<InputText>;
    m_element_init_fn["multi-slider"] = &makeWidget<MultiSlider>;
    m_element_init_fn["checkbox"] = &makeWidget<Checkbox>;
    m_element_init_fn["di-button"] = &makeWidget<Button>;
    
    m_element_init_fn["separator-text"] = &makeWidget<SeparatorText>;
    m_element_init_fn["bullet-text"] = &makeWidget<BulletText>;
    m_element_init_fn["unformatted-text"] = &makeWidget<UnformattedText>;
    m_element_init_fn["disabled-text"] = &makeWidget<DisabledText>;
    m_element_init_fn["text-wrap"] = &makeWidget<TextWrap>;
};

void ReactImgui::RenderElementById(const int id, const std::optional<ImRect>& viewport) {
    if (m_elements[id]->ShouldRender(this)) {
        m_elements[id]->m_layoutNode->SetDisplay(YGDisplayFlex);

        if (!viewport.has_value() || m_elements[id]->ShouldRenderContent(viewport)) {
            m_elements[id]->PreRender(this);
            m_elements[id]->Render(this, viewport);
            m_elements[id]->PostRender(this);
        }
    } else {
        m_elements[id]->m_layoutNode->SetDisplay(YGDisplayNone);
    }
};

void ReactImgui::RenderElements(const int id, const std::optional<ImRect>& viewport) {
    if (m_elements.contains(id)) {
        RenderElementById(id, viewport);
    }

    if (!m_elements.contains(id) || m_elements[id]->m_handlesChildrenWithinRenderMethod == false) {
        RenderChildren(id, viewport);
    }
};

void ReactImgui::RenderChildren(const int id, const std::optional<ImRect>& viewport) {
    if (m_hierarchy.contains(id)) {
        if (!m_hierarchy[id].empty()) {
            for (const auto& childId : m_hierarchy[id]) {
                RenderElements(childId, viewport);
            }
        }
    }
};

void ReactImgui::SetChildrenDisplay(const int id, const YGDisplay display) {
    if (m_hierarchy.contains(id)) {
        if (!m_hierarchy[id].empty()) {
            for (const auto& childId : m_hierarchy[id]) {
                if (m_elements.contains(childId)) {
                    m_elements[childId]->m_layoutNode->SetDisplay(display);
                }
            }
        }
    }
};

void ReactImgui::CreateElement(const json& elementDef) {
    if (elementDef.is_object()) {
        if (elementDef.contains("type") && elementDef["type"].is_string()) {
            std::string type = elementDef["type"].template get<std::string>();

            if (elementDef.contains("id") && elementDef["id"].is_number_integer()) {
                int id = elementDef["id"].template get<int>();

                if (m_element_init_fn.contains(type) || type == "node") {
                    const std::lock_guard<std::mutex> elementLock(m_elements_mutex);
                    const std::lock_guard<std::mutex> hierarchyLock(m_hierarchy_mutex);

                    try {
                        if (type == "node") {
                            m_elements[id] = makeElement(elementDef, this);
                        } else if (m_element_init_fn.contains(type)) {
                            m_elements[id] = m_element_init_fn[type](elementDef, StyledWidget::ExtractStyle(elementDef, this), this);
                        }

                        if (m_elements[id]->HasInternalOps()) {
                            m_elementInternalOpsSubject[id] = rpp::subjects::serialized_replay_subject<json>{10};
                            auto handler = [this, id](const json& opDef) {
                                m_elements[id]->HandleInternalOp(opDef);
                            };
                            m_elementInternalOpsSubject[id].get_observable() | rpp::ops::subscribe(handler);
                        }

                        m_elements[id]->Init(elementDef);

                        m_hierarchy[id] = std::vector<int>();
                    } catch (const nlohmann::json::exception& ex) {
                        // todo: signal that widget creation was not successful!
                        printf("An error occurred while decoding JSON element creation definition %d (%s): %s\n", id, type.c_str(), ex.what());
                    } catch (const std::exception& ex) {
                        // todo: signal that widget creation was not successful!
                        printf("An error occurred while creating widget %d (%s): %s\n", id, type.c_str(), ex.what());
                    }
                } else {
                    printf("unrecognised element type: '%s'\n", type.c_str());
                }
            } else {
                printf("element has no ID: '%s'\n", elementDef.dump().c_str());
            }
        } else {
            printf("received JSON does not contain type property: %s\n", elementDef.dump().c_str());
        }
    } else {
        printf("received JSON not an object\n");
    }
};

void ReactImgui::SetEventHandlers(
    const OnInitCallback onInitFn,
    const OnTextChangedCallback onInputTextChangeFn,
    const OnComboChangedCallback onComboChangeFn,
    const OnNumericValueChangedCallback onNumericValueChangeFn,
    const OnMultipleNumericValuesChangedCallback onMultiValueChangeFn,
    const OnBooleanValueChangedCallback onBooleanValueChangeFn,
    const OnClickCallback onClickFn,
    const RequestTextureFn requestTextureFn
) {
    m_onInit = onInitFn;
    m_onInputTextChange = onInputTextChangeFn;
    m_onComboChange = onComboChangeFn;
    m_onNumericValueChange = onNumericValueChangeFn;
    m_onMultiValueChange = onMultiValueChangeFn;
    m_onBooleanValueChange = onBooleanValueChangeFn;
    m_onClick = onClickFn;
    m_requestTextureFn = requestTextureFn;

    Widget::onInputTextChange_ = onInputTextChangeFn;
};

void ReactImgui::SetUpFloatFormatChars() {
    m_floatFormatChars[0] = std::make_unique<char[]>(4);
    m_floatFormatChars[1] = std::make_unique<char[]>(4);
    m_floatFormatChars[2] = std::make_unique<char[]>(4);
    m_floatFormatChars[3] = std::make_unique<char[]>(4);
    m_floatFormatChars[4] = std::make_unique<char[]>(4);
    m_floatFormatChars[5] = std::make_unique<char[]>(4);
    m_floatFormatChars[6] = std::make_unique<char[]>(4);
    m_floatFormatChars[7] = std::make_unique<char[]>(4);
    m_floatFormatChars[8] = std::make_unique<char[]>(4);
    m_floatFormatChars[9] = std::make_unique<char[]>(4);

    strcpy(m_floatFormatChars[0].get(), "%.0f");
    strcpy(m_floatFormatChars[1].get(), "%.1f");
    strcpy(m_floatFormatChars[2].get(), "%.2f");
    strcpy(m_floatFormatChars[3].get(), "%.3f");
    strcpy(m_floatFormatChars[4].get(), "%.4f");
    strcpy(m_floatFormatChars[5].get(), "%.5f");
    strcpy(m_floatFormatChars[6].get(), "%.6f");
    strcpy(m_floatFormatChars[7].get(), "%.7f");
    strcpy(m_floatFormatChars[8].get(), "%.8f");
    strcpy(m_floatFormatChars[9].get(), "%.9f");
};

void ReactImgui::Init(ImGuiRenderer* renderer) {
    m_renderer = renderer;

    if (m_rawStyleOverridesDefs.has_value()) {
        m_renderer->m_shouldLoadDefaultStyle = false;
        PatchStyle(json::parse(m_rawStyleOverridesDefs.value()));
    }

    TakeStyleSnapshot();

    PrepareForRender();
    SetUpSubjects();

    m_renderer->SetCurrentContext();

    m_onInit();
}

void ReactImgui::PrepareForRender() {
    ImGuiIO& io = m_renderer->m_imGuiCtx->IO;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;

    if (m_renderer->m_shouldLoadDefaultStyle) {
        ImGui::StyleColorsLight();
    }
};

void ReactImgui::RenderElementTree(const int id) {
    if (m_elements.contains(id)) {
        // float left = YGNodeLayoutGetLeft(m_elements[id]->m_layoutNode->m_node);
        // float top = YGNodeLayoutGetTop(m_elements[id]->m_layoutNode->m_node);
        const float width = YGNodeLayoutGetWidth(m_elements[id]->m_layoutNode->m_node);
        // float height = YGNodeLayoutGetHeight(m_elements[id]->m_layoutNode->m_node);

        if (!YGFloatIsUndefined(width)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::Text("%d", id);

            ImGui::TableNextColumn();

            ImGui::Text("%s", m_elements[id]->GetType());

            ImGui::TableNextColumn();

            ImGui::Text("%zu", m_elements[id]->m_layoutNode->GetChildCount());

            ImGui::TableNextColumn();

            if (m_elements[id]->m_isHovered) {
                ImGui::TextUnformatted("Yes");
            } else {
                ImGui::TextUnformatted("No");
            }

            ImGui::TableNextColumn();

            if (m_elements[id]->m_isActive) {
                ImGui::TextUnformatted("Yes");
            } else {
                ImGui::TextUnformatted("No");
            }

            ImGui::TableNextColumn();

            if (m_elements[id]->m_isFocused) {
                ImGui::TextUnformatted("Yes");
            } else {
                ImGui::TextUnformatted("No");
            }

            ImGui::TableNextColumn();

            ImGui::Text("%d, %d",
                (int)YGNodeLayoutGetLeft(m_elements[id]->m_layoutNode->m_node),
                (int)YGNodeLayoutGetTop(m_elements[id]->m_layoutNode->m_node)
            );

            ImGui::TableNextColumn();

            ImGui::Text("%d, %d",
                (int)YGNodeLayoutGetWidth(m_elements[id]->m_layoutNode->m_node),
                (int)YGNodeLayoutGetHeight(m_elements[id]->m_layoutNode->m_node)
            );

            ImGui::TableNextColumn();

            if (m_elements[id]->m_elementStyle.has_value()) {
                const auto style = m_elements[id]->GetElementStyleParts(m_elements[id]->GetState());

                std::string border;

                if (style->borderAll.has_value()) {
                    auto maybeResult = IV4toHEXATuple(style->borderAll.value().color);
                    if (maybeResult.has_value()) {
                        auto [borderColorHex, _] = maybeResult.value();
                        border += borderColorHex;
                    }
                }

                ImGui::Text("%s, t: %f, r: %f, b: %f, l: %f",
                border.c_str(),
                YGNodeLayoutGetBorder(m_elements[id]->m_layoutNode->m_node, YGEdgeTop),
                YGNodeLayoutGetBorder(m_elements[id]->m_layoutNode->m_node, YGEdgeRight),
                YGNodeLayoutGetBorder(m_elements[id]->m_layoutNode->m_node, YGEdgeBottom),
                YGNodeLayoutGetBorder(m_elements[id]->m_layoutNode->m_node, YGEdgeLeft)
                );
            }
        }
    }

    if (m_hierarchy.contains(id)) {
        for (const auto& childId : m_hierarchy[id]) {
            RenderElementTree(childId);
        }
    }
};

void ReactImgui::Render(const int window_width, const int window_height) {
    const std::lock_guard<std::mutex> elementsLock(m_elements_mutex);
    const std::lock_guard<std::mutex> hierarchyLock(m_hierarchy_mutex);

    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));

    ImGui::Begin(m_windowId, nullptr, m_window_flags);

    RenderElements();

    // *** DEBUG ***
    if (m_debug) {
        RenderDebugWindow();
    }
    // *** END DEBUG ***

    ImGui::End();
    ImGui::Render();
};

void ReactImgui::RenderDebugWindow() {
    ImGui::SetNextWindowSize(ImVec2(1000, 700));
    ImGui::Begin("debug", nullptr);

    if (ImGui::BeginTable("Elements", 9, ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Widget ID", ImGuiTableColumnFlags_NoHide, 35.0f);
        ImGui::TableSetupColumn("Widget Type", ImGuiTableColumnFlags_NoHide, 100.0f);
        ImGui::TableSetupColumn("Child N.", ImGuiTableColumnFlags_NoHide, 30.0f);
        ImGui::TableSetupColumn("Hovered", ImGuiTableColumnFlags_NoHide, 30.0f);
        ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_NoHide, 30.0f);
        ImGui::TableSetupColumn("Focused", ImGuiTableColumnFlags_NoHide, 30.0f);
        ImGui::TableSetupColumn("Left, Top", ImGuiTableColumnFlags_NoHide, 30.0f);
        ImGui::TableSetupColumn("Width, Height", ImGuiTableColumnFlags_NoHide, 40.0f);
        ImGui::TableSetupColumn("Border", ImGuiTableColumnFlags_NoHide, 175.0f);

        ImGui::TableHeadersRow();

        RenderElementTree();

        ImGui::EndTable();
    }
    
    ImGui::End();
}

template <typename T>
void ReactImgui::ExtractNumberFromStyleDef(const json& styleDef, const std::string key, T& value) {
    if (styleDef.contains(key) && styleDef[key].is_number_unsigned()) {
        value = styleDef[key].template get<T>();
    }
};

void ReactImgui::ExtractBooleanFromStyleDef(const json& styleDef, const std::string key, bool& value) {
    if (styleDef.contains(key) && styleDef[key].is_number_unsigned()) {
        value = styleDef[key].template get<bool>();
    }
};

void ReactImgui::ExtractImVec2FromStyleDef(const json& styleDef, const std::string key, ImVec2& value) {
    if (styleDef.contains(key)  && styleDef[key].is_array() && styleDef[key].size() == 2) {
        value.x = styleDef[key][0].template get<float>();
        value.y = styleDef[key][1].template get<float>();
    }
};

void ReactImgui::PatchStyle(const json& styleDef) {
    if (styleDef.is_object()) {
        const std::lock_guard<std::mutex> elementsLock(m_hierarchy_mutex);
        const std::lock_guard<std::mutex> hierarchyLock(m_elements_mutex);

        ImGuiStyle* style = &ImGui::GetStyle();

        ExtractNumberFromStyleDef<float>(styleDef, "alpha", style->Alpha);
        ExtractNumberFromStyleDef<float>(styleDef, "disabledAlpha", style->DisabledAlpha);
        ExtractImVec2FromStyleDef(styleDef, "windowPadding", style->WindowPadding);
        ExtractNumberFromStyleDef<float>(styleDef, "windowRounding", style->WindowRounding);
        ExtractNumberFromStyleDef<float>(styleDef, "windowBorderSize", style->WindowBorderSize);
        ExtractImVec2FromStyleDef(styleDef, "windowMinSize", style->WindowMinSize);
        ExtractImVec2FromStyleDef(styleDef, "windowTitleAlign", style->WindowTitleAlign);
        ExtractNumberFromStyleDef<int>(styleDef, "windowMenuButtonPosition", style->WindowMenuButtonPosition);
        ExtractNumberFromStyleDef<float>(styleDef, "childRounding", style->ChildRounding);
        ExtractNumberFromStyleDef<float>(styleDef, "childBorderSize", style->ChildBorderSize);
        ExtractNumberFromStyleDef<float>(styleDef, "popupRounding", style->PopupRounding);
        ExtractNumberFromStyleDef<float>(styleDef, "popupBorderSize", style->PopupBorderSize);
        ExtractImVec2FromStyleDef(styleDef, "framePadding", style->FramePadding);
        ExtractNumberFromStyleDef<float>(styleDef, "frameRounding", style->FrameRounding);
        ExtractNumberFromStyleDef<float>(styleDef, "frameBorderSize", style->FrameBorderSize);
        ExtractImVec2FromStyleDef(styleDef, "itemSpacing", style->ItemSpacing);
        ExtractImVec2FromStyleDef(styleDef, "itemInnerSpacing", style->ItemInnerSpacing);
        ExtractImVec2FromStyleDef(styleDef, "cellPadding", style->CellPadding);
        ExtractImVec2FromStyleDef(styleDef, "touchExtraPadding", style->TouchExtraPadding);
        ExtractNumberFromStyleDef<float>(styleDef, "indentSpacing", style->IndentSpacing);
        ExtractNumberFromStyleDef<float>(styleDef, "columnsMinSpacing", style->ColumnsMinSpacing);
        ExtractNumberFromStyleDef<float>(styleDef, "scrollbarSize", style->ScrollbarSize);
        ExtractNumberFromStyleDef<float>(styleDef, "scrollbarRounding", style->ScrollbarRounding);
        ExtractNumberFromStyleDef<float>(styleDef, "grabMinSize", style->GrabMinSize);
        ExtractNumberFromStyleDef<float>(styleDef, "grabRounding", style->GrabRounding);
        ExtractNumberFromStyleDef<float>(styleDef, "logSliderDeadzone", style->LogSliderDeadzone);
        ExtractNumberFromStyleDef<float>(styleDef, "tabRounding", style->TabRounding);
        ExtractNumberFromStyleDef<float>(styleDef, "tabBorderSize", style->TabBorderSize);
        ExtractNumberFromStyleDef<float>(styleDef, "tabMinWidthForCloseButton", style->TabMinWidthForCloseButton);
        ExtractNumberFromStyleDef<float>(styleDef, "tabBarBorderSize", style->TabBarBorderSize);
        ExtractNumberFromStyleDef<float>(styleDef, "tableAngledHeadersAngle", style->TableAngledHeadersAngle);
        ExtractImVec2FromStyleDef(styleDef, "tableAngledHeadersTextAlign", style->TableAngledHeadersTextAlign);
        ExtractNumberFromStyleDef<int>(styleDef, "colorButtonPosition", style->ColorButtonPosition);
        ExtractImVec2FromStyleDef(styleDef, "buttonTextAlign", style->ButtonTextAlign);
        ExtractImVec2FromStyleDef(styleDef, "selectableTextAlign", style->SelectableTextAlign);
        ExtractNumberFromStyleDef<float>(styleDef, "separatorTextBorderSize", style->SeparatorTextBorderSize);
        ExtractImVec2FromStyleDef(styleDef, "separatorTextAlign", style->SeparatorTextAlign);
        ExtractImVec2FromStyleDef(styleDef, "separatorTextPadding", style->SeparatorTextPadding);
        ExtractImVec2FromStyleDef(styleDef, "displayWindowPadding", style->DisplayWindowPadding);
        ExtractImVec2FromStyleDef(styleDef, "displaySafeAreaPadding", style->DisplaySafeAreaPadding);
        ExtractNumberFromStyleDef<float>(styleDef, "mouseCursorScale", style->MouseCursorScale);
        ExtractBooleanFromStyleDef(styleDef, "antiAliasedLines", style->AntiAliasedLines);
        ExtractBooleanFromStyleDef(styleDef, "antiAliasedLinesUseTex", style->AntiAliasedLinesUseTex);
        ExtractBooleanFromStyleDef(styleDef, "antiAliasedFill", style->AntiAliasedFill);
        ExtractNumberFromStyleDef<float>(styleDef, "curveTessellationTol", style->CurveTessellationTol);
        ExtractNumberFromStyleDef<float>(styleDef, "circleTessellationMaxError", style->CircleTessellationMaxError);
        ExtractNumberFromStyleDef<float>(styleDef, "hoverStationaryDelay", style->HoverStationaryDelay);
        ExtractNumberFromStyleDef<float>(styleDef, "hoverDelayShort", style->HoverDelayShort);
        ExtractNumberFromStyleDef<float>(styleDef, "hoverDelayNormal", style->HoverDelayNormal);
        ExtractNumberFromStyleDef<int>(styleDef, "hoverFlagsForTooltipMouse", style->HoverFlagsForTooltipMouse);
        ExtractNumberFromStyleDef<int>(styleDef, "hoverFlagsForTooltipNav", style->HoverFlagsForTooltipNav);

        if (styleDef.contains("colors") && styleDef["colors"].is_object()) {
            ImVec4* colors = style->Colors;

            for (auto& [colorItemKey, colorItemValue] : styleDef["colors"].items()) {
                auto colorItemKeyAsNumber = stoi(colorItemKey);

                if (colorItemKeyAsNumber >= 0 && colorItemKeyAsNumber < ImGuiCol_COUNT) {
                    auto maybeColor = extractColor(colorItemValue);

                    if (maybeColor.has_value()) {
                        colors[colorItemKeyAsNumber] = maybeColor.value();
                    }
                }
            }
        }
    }
};

void ReactImgui::TakeStyleSnapshot() {
    const auto style = ImGui::GetStyle();

    // This is necessary as the style is repeatedly modified during render via push and pop calls
    memcpy(&m_appStyle, &style, sizeof(style));
};

void ReactImgui::QueueCreateElement(std::string& elementJsonAsString) {
    try {
        ElementOpDef elementOp{OpCreateElement,json::parse(elementJsonAsString)};
        m_elementOpSubject.get_observer().on_next(elementOp);
    } catch (nlohmann::detail::parse_error& parseError) {
        printf("ReactImgui::QueueCreateElement, parse error: %s\n", parseError.what());
    }
};

void ReactImgui::QueuePatchElement(const int id, std::string& elementJsonAsString) {
    try {
        json opDef = json::parse(elementJsonAsString);
        opDef["id"] = id;
        ElementOpDef elementOp{OpPatchElement,opDef};
        m_elementOpSubject.get_observer().on_next(elementOp);
    } catch (nlohmann::detail::parse_error& parseError) {
        printf("ReactImgui::QueuePatchElement, parse error: %s\n", parseError.what());
    }
};

void ReactImgui::QueueAppendChild(int parentId, int childId) {
    try {
        json opDef;
        opDef["parentId"] = parentId;
        opDef["childId"] = childId;
        ElementOpDef elementOp{OpAppendChild,opDef};
        m_elementOpSubject.get_observer().on_next(elementOp);
    } catch (nlohmann::detail::parse_error& parseError) {
        printf("ReactImgui::QueueAppendChild, parse error: %s\n", parseError.what());
    }
};

void ReactImgui::QueueSetChildren(const int parentId, const std::vector<int>& childrenIds) {
    try {
        json opDef;
        opDef["parentId"] = parentId;
        opDef["childrenIds"] = childrenIds;
        ElementOpDef elementOp{OpSetChildren,opDef};
        m_elementOpSubject.get_observer().on_next(elementOp);
    } catch (nlohmann::detail::parse_error& parseError) {
        printf("ReactImgui::QueueSetChildren, parse error: %s\n", parseError.what());
    }
};

void ReactImgui::QueueElementInternalOp(const int id, std::string& widgetOpDef) {
    try {
        const json opDef = json::parse(widgetOpDef);

        if (m_elementInternalOpsSubject.contains(id)) {
            m_elementInternalOpsSubject[id].get_observer().on_next(opDef);
        }
    } catch (nlohmann::detail::parse_error& parseError) {
        printf("ReactImgui::QueueElementInternalOp, parse error: %s\n", parseError.what());
    }
};

void ReactImgui::PatchElement(const json& patchDef) {
    if (patchDef.is_object()) {
        const auto id = patchDef["id"].template get<int>();

        const std::lock_guard<std::mutex> lock(m_elements_mutex);

        if (m_elements.contains(id)) {
            const auto pElement = m_elements[id].get();

            try {
                pElement->Patch(patchDef, this);
            } catch (const nlohmann::json::exception& ex) {
                // todo: signal that widget creation was not successful!
                printf("An error occurred while decoding JSON element patching definition %d (%s): %s\n", id, pElement->m_type.c_str(), ex.what());
            } catch (const std::exception& ex) {
                // todo: signal that widget creation was not successful!
                printf("An error occurred while patching widget %d (%s): %s\n", id, pElement->m_type.c_str(), ex.what());
            }
        }
    }
}

void ReactImgui::HandleElementInternalOp(const json& opDef) {
    const auto id = opDef["id"].template get<int>();

    const std::lock_guard<std::mutex> lock(m_elements_mutex);

    if (m_elements.contains(id)) {
        const auto pElement = m_elements[id].get();

        try {
            pElement->HandleInternalOp(opDef);
        } catch (const nlohmann::json::exception& ex) {
            // todo: signal that widget creation was not successful!
            printf("An error occurred while decoding JSON element internal op definition %d (%s): %s\n", id, pElement->m_type.c_str(), ex.what());
        } catch (const std::exception& ex) {
            // todo: signal that widget creation was not successful!
            printf("An error occurred while invoking widget %d (%s)'s interal op (%s): %s\n", id, pElement->m_type.c_str(), opDef.dump().c_str(), ex.what());
        }
    }
}

void ReactImgui::SetChildren(const json& opDef) {
    const std::lock_guard<std::mutex> elementsLock(m_hierarchy_mutex);
    const std::lock_guard<std::mutex> hierarchyLock(m_elements_mutex);

    auto parentId = opDef["parentId"].template get<int>();
    auto childrenIds = opDef["childrenIds"].template get<std::vector<int>>();

    if (m_elements.contains(parentId)) {
        YGNodeRemoveAllChildren(m_elements[parentId]->m_layoutNode->m_node);

        const auto size = childrenIds.size();

        for (int i = 0; i < size; i++) {
            const auto childId = childrenIds[i];

            if (m_elements.contains(childId)) {
                m_elements[parentId]->m_layoutNode->InsertChild(m_elements[childId]->m_layoutNode.get(), i);
            }
        }
    }

    m_hierarchy[parentId] = childrenIds;
}

void ReactImgui::AppendChild(const json& opDef) {
    auto parentId = opDef["parentId"].template get<int>();
    auto childId = opDef["childId"].template get<int>();

    const std::lock_guard<std::mutex> lock(m_hierarchy_mutex);
    
    if (m_hierarchy.contains(parentId)) {
        if ( std::find(m_hierarchy[parentId].begin(), m_hierarchy[parentId].end(), childId) == m_hierarchy[parentId].end() ) {
            const std::lock_guard<std::mutex> elementsLock(m_elements_mutex);
    
            if (m_elements.contains(childId)) {
                if (!m_elements[childId]->m_isRoot) {
                    auto parentNode = YGNodeGetParent(m_elements[childId]->m_layoutNode->m_node);
    
                    if (!parentNode) {
                        const auto childCount = m_elements[parentId]->m_layoutNode->GetChildCount();
    
                        m_elements[parentId]->m_layoutNode->InsertChild(m_elements[childId]->m_layoutNode.get(), childCount);
                    }
                }
    
                m_hierarchy[parentId].push_back(childId);
            }
        }
    }
}

std::vector<int> ReactImgui::GetChildren(int id) {
    return m_hierarchy[id];
};

// todo: switch to ReactivePlusPlus's BehaviorSubject
void ReactImgui::AppendTextToClippedMultiLineTextRenderer(const int id, const std::string& data) {
    const std::lock_guard<std::mutex> lock(m_elements_mutex);

    if (m_elements.contains(id)) {
        if (m_elements[id]->m_type == "clipped-multi-line-text-renderer") {
            dynamic_cast<ClippedMultiLineTextRenderer*>(m_elements[id].get())->AppendText(data.c_str());
        }
    }
};

StyleVarValueRef ReactImgui::GetStyleVar(const ImGuiStyleVar key) {
    StyleVarValueRef value;

    switch(key) {
        case ImGuiStyleVar_Alpha: value.emplace<const float*>(&m_appStyle.Alpha); break;
        case ImGuiStyleVar_DisabledAlpha: value.emplace<const float*>(&m_appStyle.DisabledAlpha); break;
        case ImGuiStyleVar_WindowPadding: value.emplace<const ImVec2*>(&m_appStyle.WindowPadding); break;
        case ImGuiStyleVar_WindowRounding: value.emplace<const float*>(&m_appStyle.WindowRounding); break;
        case ImGuiStyleVar_WindowBorderSize: value.emplace<const float*>(&m_appStyle.WindowBorderSize); break;
        case ImGuiStyleVar_WindowMinSize: value.emplace<const ImVec2*>(&m_appStyle.WindowMinSize); break;
        case ImGuiStyleVar_WindowTitleAlign: value.emplace<const ImVec2*>(&m_appStyle.WindowTitleAlign); break;
        case ImGuiStyleVar_ChildRounding: value.emplace<const float*>(&m_appStyle.ChildRounding); break;
        case ImGuiStyleVar_ChildBorderSize: value.emplace<const float*>(&m_appStyle.ChildBorderSize); break;
        case ImGuiStyleVar_PopupRounding: value.emplace<const float*>(&m_appStyle.PopupRounding); break;
        case ImGuiStyleVar_PopupBorderSize: value.emplace<const float*>(&m_appStyle.PopupBorderSize); break;
        case ImGuiStyleVar_FramePadding: value.emplace<const ImVec2*>(&m_appStyle.FramePadding); break;
        case ImGuiStyleVar_FrameRounding: value.emplace<const float*>(&m_appStyle.FrameRounding); break;
        case ImGuiStyleVar_FrameBorderSize: value.emplace<const float*>(&m_appStyle.FrameBorderSize); break;
        case ImGuiStyleVar_ItemSpacing: value.emplace<const ImVec2*>(&m_appStyle.ItemSpacing); break;
        case ImGuiStyleVar_ItemInnerSpacing: value.emplace<const ImVec2*>(&m_appStyle.ItemInnerSpacing); break;
        case ImGuiStyleVar_IndentSpacing: value.emplace<const float*>(&m_appStyle.IndentSpacing); break;
        case ImGuiStyleVar_CellPadding: value.emplace<const ImVec2*>(&m_appStyle.CellPadding); break;
        case ImGuiStyleVar_ScrollbarSize: value.emplace<const float*>(&m_appStyle.ScrollbarSize); break;
        case ImGuiStyleVar_ScrollbarRounding: value.emplace<const float*>(&m_appStyle.ScrollbarRounding); break;
        case ImGuiStyleVar_GrabMinSize: value.emplace<const float*>(&m_appStyle.GrabMinSize); break;
        case ImGuiStyleVar_GrabRounding: value.emplace<const float*>(&m_appStyle.GrabRounding); break;
        case ImGuiStyleVar_TabRounding: value.emplace<const float*>(&m_appStyle.TabRounding); break;
        case ImGuiStyleVar_TabBorderSize: value.emplace<const float*>(&m_appStyle.TabBorderSize); break;
        case ImGuiStyleVar_TabBarBorderSize: value.emplace<const float*>(&m_appStyle.TabBarBorderSize); break;
        case ImGuiStyleVar_TableAngledHeadersAngle: value.emplace<const float*>(&m_appStyle.TableAngledHeadersAngle); break;
        case ImGuiStyleVar_TableAngledHeadersTextAlign: value.emplace<const ImVec2*>(&m_appStyle.TableAngledHeadersTextAlign); break;
        case ImGuiStyleVar_ButtonTextAlign: value.emplace<const ImVec2*>(&m_appStyle.ButtonTextAlign); break;
        case ImGuiStyleVar_SelectableTextAlign: value.emplace<const ImVec2*>(&m_appStyle.SelectableTextAlign); break;
        case ImGuiStyleVar_SeparatorTextBorderSize: value.emplace<const float*>(&m_appStyle.SeparatorTextBorderSize); break;
        case ImGuiStyleVar_SeparatorTextAlign: value.emplace<const ImVec2*>(&m_appStyle.SeparatorTextAlign); break;
        case ImGuiStyleVar_SeparatorTextPadding: value.emplace<const ImVec2*>(&m_appStyle.SeparatorTextPadding); break;
        default: break;
    }

    return value;
};

// todo: ensure this returns the font based on current state of the widget, i.e. 'base', 'hover', 'active'
ImFont* ReactImgui::GetWidgetFont(const StyledWidget* widget) {
    if (widget->HasCustomStyles() && widget->HasCustomFont(this)) {
        // auto result = widget->m_style.value()->GetCustomFontId(widget->GetState(), this);

        // if (result.has_value()) {
            return m_renderer->m_loadedFonts[widget->m_style.value()->GetCustomFontId(widget->GetState(), this)];
        // }
    }

    ImGuiIO& io = m_renderer->m_imGuiCtx->IO;

    // Return default font size as we might be in the middle of rendering a widget with a custom font
    return io.FontDefault;
}

// todo: ensure this returns the font size based on current state of the widget, i.e. 'base', 'hover', 'active'
float ReactImgui::GetWidgetFontSize(const StyledWidget* widget) {
    ImGui::CreateContext();

    if (widget->HasCustomStyles() && widget->HasCustomFont(this)) {
        // auto result = widget->m_style.value()->GetCustomFontId(widget->GetState(), this);

        // if (result.has_value()) {
            return m_renderer->m_loadedFonts[widget->m_style.value()->GetCustomFontId(widget->GetState(), this)]->FontSize;
        // }
    }

    // return 16.0f;

    ImGuiIO& io = m_renderer->m_imGuiCtx->IO;

    if (!io.FontDefault || !io.FontDefault->FontSize) {
        return 16.0f;
    }

    // Return default font size as we might be in the middle of rendering a widget with a custom font
    return io.FontDefault->FontSize;
}

float ReactImgui::GetTextLineHeight(const StyledWidget* widget) {
    return GetWidgetFontSize(widget);
};

float ReactImgui::GetTextLineHeightWithSpacing(const StyledWidget* widget) {
    auto fontSize = GetWidgetFontSize(widget);

    float itemSpacingY = m_appStyle.ItemSpacing.y;

    if (widget->HasCustomStyles() && widget->HasCustomStyleVar(ImGuiStyleVar_ItemSpacing)) {
        auto maybeCustomItemSpacing = widget->GetCustomStyleVar(ImGuiStyleVar_ItemSpacing);
        if (std::holds_alternative<ImVec2>(maybeCustomItemSpacing)) {
            itemSpacingY = std::get<ImVec2>(maybeCustomItemSpacing).y;
        }
    }

    return fontSize + itemSpacingY;
};

float ReactImgui::GetFrameHeight(const StyledWidget* widget) {
    auto fontSize = GetWidgetFontSize(widget);

    float framePaddingY = m_appStyle.FramePadding.y;

    if (widget->HasCustomStyles() && widget->HasCustomStyleVar(ImGuiStyleVar_FramePadding)) {
        auto maybeCustomFramePadding = widget->GetCustomStyleVar(ImGuiStyleVar_FramePadding);
        if (std::holds_alternative<ImVec2>(maybeCustomFramePadding)) {
            framePaddingY = std::get<ImVec2>(maybeCustomFramePadding).y;
        }
    }

    return fontSize + framePaddingY * 2.0f;
};

float ReactImgui::GetFrameHeightWithSpacing(const StyledWidget* widget) {
    auto fontSize = GetWidgetFontSize(widget);

    float framePaddingY = m_appStyle.FramePadding.y;
    float itemSpacingY = m_appStyle.ItemSpacing.y;

    if (widget->HasCustomStyles()) {
        if (widget->HasCustomStyleVar(ImGuiStyleVar_FramePadding)) {
            auto maybeCustomFramePadding = widget->GetCustomStyleVar(ImGuiStyleVar_FramePadding);
            if (std::holds_alternative<ImVec2>(maybeCustomFramePadding)) {
                framePaddingY = std::get<ImVec2>(maybeCustomFramePadding).y;
            }
        }

        if (widget->HasCustomStyleVar(ImGuiStyleVar_ItemSpacing)) {
            auto maybeCustomItemSpacing = widget->GetCustomStyleVar(ImGuiStyleVar_ItemSpacing);
            if (std::holds_alternative<ImVec2>(maybeCustomItemSpacing)) {
                itemSpacingY = std::get<ImVec2>(maybeCustomItemSpacing).y;
            }
        }
    }

    return fontSize + framePaddingY * 2.0f + itemSpacingY;
};

ImVec2 ReactImgui::CalcTextSize(const StyledWidget* widget, const char* text, const char* text_end, bool hide_text_after_double_hash, float wrap_width)
{
    auto font = GetWidgetFont(widget);
    const char* text_display_end;

    if (hide_text_after_double_hash) {
        text_display_end = ImGui::FindRenderedTextEnd(text, text_end);      // Hide anything after a '##' string
    } else {
        text_display_end = text_end;
    }

    const float font_size = font->FontSize;
    if (text == text_display_end)
        return ImVec2(0.0f, font_size);
    ImVec2 text_size = font->CalcTextSizeA(font->FontSize, FLT_MAX, wrap_width, text, text_display_end, NULL);

    // Round
    // FIXME: This has been here since Dec 2015 (7b0bf230) but down the line we want this out.
    // FIXME: Investigate using ceilf or e.g.
    // - https://git.musl-libc.org/cgit/musl/tree/src/math/ceilf.c
    // - https://embarkstudios.github.io/rust-gpu/api/src/libm/math/ceilf.rs.html
    text_size.x = IM_TRUNC(text_size.x + 0.99999f);

    return text_size;
}