#include <cstring>
#include <string>
#include <functional>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <chrono>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include <GLFW/glfw3.h>
#include "implot.h"
#include "implot_internal.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#include "imgui_impl_wgpu.h"
#else
#include <GLES3/gl3.h>
#include "imgui_impl_opengl3.h"
#endif

#include <rpp/rpp.hpp>
#include <nlohmann/json.hpp>

#include "element/layout_node.h"

#include "shared.h"
#include "implot_renderer.h"
#include "xframes.h"

#include "color_helpers.h"
#include "implot_renderer.h"

#include "widget/js_canvas.h"
#include "widget/lua_canvas.h"
#include "widget/janet_canvas.h"
#include "widget/map_view.h"

#include "widget/button.h"
#include "widget/color_indicator.h"
#include "widget/checkbox.h"
#include "widget/color_picker.h"
#include "widget/child.h"
#include "widget/clipped_multi_line_text_renderer.h"
#include "widget/collapsing_header.h"
#include "widget/combo.h"
#include "widget/group.h"
#include "widget/image.h"
#include "widget/input_text.h"
#include "widget/item_tooltip.h"
#include "widget/multi_slider.h"
#include "widget/plot_bar.h"
#include "widget/plot_histogram.h"
#include "widget/plot_pie_chart.h"
#include "widget/progress_bar.h"
#include "widget/plot_candlestick.h"
#include "widget/plot_heatmap.h"
#include "widget/plot_line.h"
#include "widget/plot_scatter.h"
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
std::unique_ptr<T> makeWidget(const json& val, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
    return T::makeWidget(val, maybeStyle, view);
}

std::unique_ptr<Element> makeElement(const json& val, XFrames* view) {
    return Element::makeElement(val, view);
}

XFrames::XFrames(
    const char* windowId,
    std::optional<std::string> rawStyleOverridesDefs
) {
    m_windowId = windowId;
    m_debug = false;
    m_rawStyleOverridesDefs = std::move(rawStyleOverridesDefs);
    m_hierarchy.emplace(0, std::vector<int>{});

    SetUpElementCreatorFunctions();
    SetUpFloatFormatChars();
    SetUpSubjects();
}

XFrames::~XFrames() {
    Dispose();
}

void XFrames::Dispose() {
    const std::lock_guard dispatchLock(m_commitMutex);
    if (m_runtimeDisposed.exchange(true)) return;
    m_frameScheduler.Dispose();
    m_commitSubscription.dispose();
    m_elementOpSubject.get_disposable().dispose();
    const std::lock_guard hierarchyLock(m_hierarchy_mutex);
    const std::lock_guard elementsLock(m_elements_mutex);
    // Break all Yoga parent-child links before m_elements map destructs.
    // unordered_map destroys entries in arbitrary order; if a parent is freed
    // before its child, YGNodeFree(child) calls owner->removeChild() on the
    // already-freed parent's YGNode — use-after-free.
    // YGNodeRemoveAllChildren sets child->setOwner(nullptr), so subsequent
    // YGNodeFree calls safely skip the removeChild path.
    for (auto& [id, element] : m_elements) {
        if (element && element->m_layoutNode && element->m_layoutNode->m_node) {
            YGNodeRemoveAllChildren(element->m_layoutNode->m_node);
        }
    }
    // Close resource mailboxes while their runtime worker/renderer owners still
    // exist; member destruction order alone destroys those owners first.
    m_elements.clear();
    m_elementInternalOpsSubject.clear();
    m_publicationOwnedIds.clear();
    m_hierarchy.clear();
    m_hierarchy.emplace(0, std::vector<int>{});
    m_diagnosticsLastInternalOpMs.clear();
    m_floatFormatChars.clear();
    m_pendingFrame.reset();
    m_pendingDiagnosticsFrame = nullptr;
    { const std::lock_guard eventLock(m_resourceEventMutex); m_prefetchEvents.clear(); }
#ifndef __EMSCRIPTEN__
    m_mapWorker.Stop();
#endif
}

void XFrames::SetDebug(bool debug) {
    {
        const std::lock_guard lock(m_elements_mutex);
        m_debug = debug;
        m_debugFocusRequested = debug;
        m_frameScheduler.Invalidate(xframes::FrameReason::Diagnostics);
    }
    m_frameScheduler.Notify();
};

void XFrames::ShowDebugWindow() {
    {
        const std::lock_guard lock(m_elements_mutex);
        m_debugFocusRequested = true;
        m_frameScheduler.Invalidate(xframes::FrameReason::Diagnostics);
    }
    m_frameScheduler.Notify();
};

double XFrames::DiagnosticsNowMs() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void XFrames::SetDiagnosticsEnabled(bool enabled) {
    // A newly enabled snapshot requests one frame. Reads and disabling are pure
    // observations and never reset native ordering or discard pending work.
    const auto previous = m_diagnosticsEnabled.exchange(enabled, std::memory_order_relaxed);
    if (enabled && !previous) {
        const std::lock_guard<std::mutex> lock(m_elements_mutex);
        m_frameScheduler.Invalidate(xframes::FrameReason::Diagnostics);
    }
    if (enabled && !previous) m_frameScheduler.Notify();
}

json XFrames::GetDiagnosticsFrame() {
    const std::lock_guard<std::mutex> lock(m_diagnosticsMutex);
    auto result = m_diagnosticsFrame;
    result["enabled"] = m_diagnosticsEnabled.load(std::memory_order_relaxed);
    result["sampledAtMs"] = DiagnosticsNowMs();
    result["scheduler"] = m_frameScheduler.GetState();
    result["platform"] = m_renderer ? m_renderer->GetPlatformDiagnostics() : json::object();
    result["surfaceStatus"] = m_runtimeDisposed ? "disposed" : m_surfaceQuarantined ? "quarantined" : "healthy";
    result["resourceState"] = {{"textures", m_renderer ? m_renderer->GetResourceDiagnostics() : json::object()}};
#ifndef __EMSCRIPTEN__
    result["resourceState"]["mapWorkers"] = m_mapWorker.Diagnostics();
#endif
    { const std::lock_guard eventLock(m_resourceEventMutex); result["resourceState"]["queuedPrefetchEvents"] = m_prefetchEvents.size(); }
    return result;
}

json XFrames::GetDiagnosticsState() {
    const std::lock_guard<std::mutex> hierarchyLock(m_hierarchy_mutex);
    const std::lock_guard<std::mutex> elementsLock(m_elements_mutex);
    if (m_surfaceQuarantined) throw xframes::CommitError("surface_quarantined", "Publication failed; recreate the native runtime");
    return BuildDiagnosticsStateUnlocked();
}

json XFrames::BuildDiagnosticsStateUnlocked() {
    std::vector<int> ids;
    std::unordered_map<YGNodeConstRef, int> yogaIds;
    for (const auto& [id, element] : m_elements) {
        ids.push_back(id);
        if (element && element->m_layoutNode) yogaIds[element->m_layoutNode->m_node] = id;
    }
    std::sort(ids.begin(), ids.end());
    std::unordered_set<int> reachable;
    std::vector<int> pending{0};
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        if (!reachable.insert(id).second) continue;
        const auto it = m_hierarchy.find(id);
        if (it != m_hierarchy.end()) pending.insert(pending.end(), it->second.begin(), it->second.end());
    }
    json elements = json::array();
    size_t unreachableCount = 0;
    for (int id : ids) {
        if (!reachable.contains(id)) ++unreachableCount;
        if (elements.size() >= 4096) continue;
        const auto& element = m_elements.at(id);
        if (!element) { elements.push_back({{"id", id}, {"invalid", true}}); continue; }
        const auto node = element->m_layoutNode->m_node;
        const auto owner = YGNodeGetOwner(node);
        json yogaChildren = json::array();
        for (uint32_t i = 0; i < YGNodeGetChildCount(node); ++i) {
            const auto it = yogaIds.find(YGNodeGetChild(node, i));
            yogaChildren.push_back(it == yogaIds.end() ? json(nullptr) : json(it->second));
        }
        const auto parentIt = yogaIds.find(owner);
        const auto hierarchyIt = m_hierarchy.find(id);
        json record = {{"id", id}, {"type", element->m_type}, {"reachable", reachable.contains(id)},
            {"children", hierarchyIt == m_hierarchy.end() ? std::vector<int>{} : hierarchyIt->second},
            {"yogaParent", parentIt == yogaIds.end() ? json(nullptr) : json(parentIt->second)},
            {"yogaChildren", yogaChildren},
            {"bounds", {YGNodeLayoutGetLeft(node), YGNodeLayoutGetTop(node), YGNodeLayoutGetWidth(node), YGNodeLayoutGetHeight(node)}}};
        const auto opIt = m_diagnosticsLastInternalOpMs.find(id);
        record["lastInternalOpMs"] = opIt == m_diagnosticsLastInternalOpMs.end() ? json(nullptr) : json(opIt->second);
        const auto resources = element->GetResourceDiagnostics();
        if (!resources.is_null()) record["resources"] = resources;
        if (auto* plot = dynamic_cast<PlotBar*>(element.get())) record["state"] = plot->GetDiagnosticsState();
        if (auto* table = dynamic_cast<Table*>(element.get())) {
            record["state"] = {{"rowCount", table->m_data.size()}, {"columnCount", table->m_columns.size()},
                {"filterDirty", table->m_filterDirty}, {"filteredCount", table->m_filteredIndices.size()},
                {"firstRow", table->m_data.empty() ? json(nullptr) : json(table->m_data.front())},
                {"lastRow", table->m_data.empty() ? json(nullptr) : json(table->m_data.back())}};
        }
        elements.push_back(std::move(record));
    }
    return {{"elementCount", m_elements.size()}, {"hierarchyCount", m_hierarchy.size()},
        {"internalSubjectCount", m_elementInternalOpsSubject.size()}, {"unreachableCount", unreachableCount},
        {"truncatedElements", ids.size() > 4096}, {"elements", elements},
        {"rootChildren", m_hierarchy.contains(0) ? m_hierarchy.at(0) : std::vector<int>{}}};
}

void XFrames::CompleteDiagnosticsFrame() {
    // The pending state was captured while constructing draw data under the tree locks.
    // Publication after submission does not inspect a potentially newer live tree.
    if (!m_pendingFrame || !m_frameScheduler.Complete(*m_pendingFrame)) {
        m_pendingFrame.reset();
        m_pendingDiagnosticsFrame = nullptr;
        return;
    }
    const auto frame = *m_pendingFrame;
    m_pendingFrame.reset();
    if (m_pendingDiagnosticsFrame.is_null() || !m_diagnosticsEnabled.load(std::memory_order_relaxed)) {
        m_pendingDiagnosticsFrame = nullptr;
        return;
    }
    m_pendingDiagnosticsFrame["frameId"] = std::to_string(frame.id);
    m_pendingDiagnosticsFrame["nativeRevision"] = std::to_string(frame.revision);
    m_pendingDiagnosticsFrame["coveredGeneration"] = std::to_string(frame.generation);
    m_pendingDiagnosticsFrame["reasons"] = frame.reasons;
    m_pendingDiagnosticsFrame["submittedAtMs"] = DiagnosticsNowMs();
    m_pendingDiagnosticsFrame["backend"] = m_renderer->GetDiagnosticsBackendInfo();
    const std::lock_guard<std::mutex> lock(m_diagnosticsMutex);
    m_diagnosticsFrame = std::move(m_pendingDiagnosticsFrame);
    m_pendingDiagnosticsFrame = nullptr;
}

void XFrames::QueuePrefetchProgress(xframes::FrameScheduler::Source source, int id, int completed, int total) {
    const std::lock_guard lock(m_resourceEventMutex);
    m_prefetchEvents.insert_or_assign(id, PrefetchEvent{std::move(source), id, completed, total});
}

void XFrames::FlushResourceEvents() {
    std::unordered_map<int, PrefetchEvent> events;
    {
        const std::lock_guard lock(m_resourceEventMutex);
        events.swap(m_prefetchEvents);
    }
    for (const auto& [id, event] : events) {
        if (!event.source.IsAlive()) continue;
        if (m_onGuardedPrefetchProgress) m_onGuardedPrefetchProgress(event.source, id, event.completed, event.total);
        else if (m_onPrefetchProgress) m_onPrefetchProgress(id, event.completed, event.total);
    }
}

void XFrames::AbandonFrame() {
    if (m_pendingFrame) m_frameScheduler.Abandon(*m_pendingFrame);
    m_pendingFrame.reset();
    m_pendingDiagnosticsFrame = nullptr;
}

void XFrames::SetUpSubjects() {
    // Idempotent: Init/tests must never reset a live instance's ordering state.
    const std::lock_guard<std::mutex> lock(m_commitMutex);
    if (m_subjectsReady) return;
    m_elementOpSubject = rpp::subjects::serialized_replay_subject<std::weak_ptr<CommitRequest>>{1};
    m_elementOpSubject.get_observable() | rpp::ops::subscribe(m_commitSubscription,
        [this](const std::weak_ptr<CommitRequest>& record) {
            if (auto request = record.lock()) {
                // RPP must not convert an application exception into a terminated
                // subscription. Transfer it back to this synchronous caller instead.
                try { request->result = ApplyCommitOperations(request->batch); }
                catch (...) { request->exception = std::current_exception(); }
                request->completed = true;
            }
        });
    m_subjectsReady = true;
}

void XFrames::SetUpElementCreatorFunctions() {
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

    m_element_init_fn["map-view"] = &makeWidget<MapView>;
    m_element_init_fn["di-js-canvas"] = &makeWidget<JsCanvas>;
    m_element_init_fn["di-lua-canvas"] = &makeWidget<LuaCanvas>;
    m_element_init_fn["di-janet-canvas"] = &makeWidget<JanetCanvas>;

    m_element_init_fn["plot-bar"] = &makeWidget<PlotBar>;
    m_element_init_fn["plot-heatmap"] = &makeWidget<PlotHeatmap>;
    m_element_init_fn["plot-histogram"] = &makeWidget<PlotHistogram>;
    m_element_init_fn["plot-line"] = &makeWidget<PlotLine>;
    m_element_init_fn["plot-pie-chart"] = &makeWidget<PlotPieChart>;
    m_element_init_fn["plot-scatter"] = &makeWidget<PlotScatter>;
    m_element_init_fn["plot-candlestick"] = &makeWidget<PlotCandlestick>;

    m_element_init_fn["item-tooltip"] = &makeWidget<ItemTooltip>;

    m_element_init_fn["color-indicator"] = &makeWidget<ColorIndicator>;
    m_element_init_fn["combo"] = &makeWidget<Combo>;
    m_element_init_fn["slider"] = &makeWidget<Slider>;
    m_element_init_fn["input-text"] = &makeWidget<InputText>;
    m_element_init_fn["multi-slider"] = &makeWidget<MultiSlider>;
    m_element_init_fn["checkbox"] = &makeWidget<Checkbox>;
    m_element_init_fn["color-picker"] = &makeWidget<ColorPicker>;
    m_element_init_fn["di-button"] = &makeWidget<Button>;

    m_element_init_fn["progress-bar"] = &makeWidget<ProgressBar>;
    m_element_init_fn["separator-text"] = &makeWidget<SeparatorText>;
    m_element_init_fn["bullet-text"] = &makeWidget<BulletText>;
    m_element_init_fn["unformatted-text"] = &makeWidget<UnformattedText>;
    m_element_init_fn["disabled-text"] = &makeWidget<DisabledText>;
    m_element_init_fn["text-wrap"] = &makeWidget<TextWrap>;
};

void XFrames::RenderElementById(const int id, const std::optional<ImRect>& viewport) {
    auto* el = m_elements[id].get();
    if (el->ShouldRender(this)) {
        el->m_layoutNode->SetDisplay(YGDisplayFlex);

        if (!viewport.has_value() || el->ShouldRenderContent(viewport)) {
            el->PreRender(this);
            el->Render(this, viewport);
            el->PostRender(this);
        }
    } else {
        el->m_layoutNode->SetDisplay(YGDisplayNone);
    }
};

void XFrames::RenderElements(const int id, const std::optional<ImRect>& viewport) {
    auto it = m_elements.find(id);
    if (it != m_elements.end()) {
        RenderElementById(id, viewport);
        if (!it->second->m_handlesChildrenWithinRenderMethod) {
            RenderChildren(id, viewport);
        }
    } else {
        RenderChildren(id, viewport);
    }
};

void XFrames::RenderChildren(const int id, const std::optional<ImRect>& viewport) {
    auto it = m_hierarchy.find(id);
    if (it != m_hierarchy.end() && !it->second.empty()) {
        for (const auto& childId : it->second) {
            RenderElements(childId, viewport);
        }
    }
};

void XFrames::SetChildrenDisplay(const int id, const YGDisplay display) {
    auto hIt = m_hierarchy.find(id);
    if (hIt != m_hierarchy.end() && !hIt->second.empty()) {
        for (const auto& childId : hIt->second) {
            auto eIt = m_elements.find(childId);
            if (eIt != m_elements.end()) {
                eIt->second->m_layoutNode->SetDisplay(display);
            }
        }
    }
};

void XFrames::CreateElementUnlocked(const json& elementDef) {
    const auto id = elementDef.at("id").get<int>();
    const auto type = elementDef.at("type").get<std::string>();
    if (type == "node") {
        m_elements[id] = makeElement(elementDef, this);
    } else if (m_element_init_fn.contains(type)) {
        m_elements[id] = m_element_init_fn[type](elementDef, StyledWidget::ExtractStyle(elementDef, this), this);
    }

    if (m_elements[id]->HasInternalOps()) {
        m_elementInternalOpsSubject[id] = rpp::subjects::serialized_replay_subject<json>{10};
        const auto owner = m_elementInternalOpsSubject.at(id).get_disposable().as_weak();
        auto handler = [this, id, owner](const json& opDef) {
            const std::lock_guard<std::mutex> lock(m_elements_mutex);
            const auto current = m_elementInternalOpsSubject.find(id);
            if (!m_surfaceQuarantined && m_elements.contains(id) && current != m_elementInternalOpsSubject.end()
                && current->second.get_disposable() == owner) {
                m_elements[id]->HandleInternalOp(opDef);
                m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
                if (m_diagnosticsEnabled.load(std::memory_order_relaxed)) {
                    m_diagnosticsLastInternalOpMs[id] = DiagnosticsNowMs();
                }
            }
        };
        m_elementInternalOpsSubject[id].get_observable() | rpp::ops::subscribe(handler);
    }

    m_elements[id]->Init(elementDef);

    m_hierarchy[id] = std::vector<int>();
}

void XFrames::SetEventHandlers(
    const OnInitCallback onInitFn,
    const OnTextChangedCallback onInputTextChangeFn,
    const OnComboChangedCallback onComboChangeFn,
    const OnNumericValueChangedCallback onNumericValueChangeFn,
    const OnMultipleNumericValuesChangedCallback onMultiValueChangeFn,
    const OnBooleanValueChangedCallback onBooleanValueChangeFn,
    const OnClickCallback onClickFn,
    const OnTableSortCallback onTableSortFn,
    const OnTableFilterCallback onTableFilterFn,
    const OnTableRowClickCallback onTableRowClickFn,
    const OnTableItemActionCallback onTableItemActionFn,
    const OnPrefetchProgressCallback onPrefetchProgressFn,
    const OnScriptErrorCallback onScriptErrorFn
) {
    m_onInit = onInitFn;
    m_onInputTextChange = onInputTextChangeFn;
    m_onComboChange = onComboChangeFn;
    m_onNumericValueChange = onNumericValueChangeFn;
    m_onMultiValueChange = onMultiValueChangeFn;
    m_onBooleanValueChange = onBooleanValueChangeFn;
    m_onClick = onClickFn;
    m_onTableSort = onTableSortFn;
    m_onTableFilter = onTableFilterFn;
    m_onTableRowClick = onTableRowClickFn;
    m_onTableItemAction = onTableItemActionFn;
    m_onPrefetchProgress = onPrefetchProgressFn;
    m_onScriptError = onScriptErrorFn;

    Widget::onInputTextChange_ = onInputTextChangeFn;
};

void XFrames::SetUpFloatFormatChars() {
    m_floatFormatChars[0] = std::make_unique<char[]>(5);
    m_floatFormatChars[1] = std::make_unique<char[]>(5);
    m_floatFormatChars[2] = std::make_unique<char[]>(5);
    m_floatFormatChars[3] = std::make_unique<char[]>(5);
    m_floatFormatChars[4] = std::make_unique<char[]>(5);
    m_floatFormatChars[5] = std::make_unique<char[]>(5);
    m_floatFormatChars[6] = std::make_unique<char[]>(5);
    m_floatFormatChars[7] = std::make_unique<char[]>(5);
    m_floatFormatChars[8] = std::make_unique<char[]>(5);
    m_floatFormatChars[9] = std::make_unique<char[]>(5);

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

void XFrames::Init(ImGuiRenderer* renderer) {
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

void XFrames::PrepareForRender() {
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

void XFrames::RenderElementTree(const int id) {
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

bool XFrames::Render(int window_width, int window_height) {
    const std::lock_guard<std::mutex> hierarchyLock(m_hierarchy_mutex);
    const std::lock_guard<std::mutex> elementsLock(m_elements_mutex);

    AbandonFrame();
    m_pendingFrame = m_frameScheduler.Capture(m_nativeRevision);
    if (!m_pendingFrame) return false;
    // Capture precedes all render-thread work. Any producer arriving after this
    // point remains pending, even if preparation also consumes its newer work.
    if (!m_renderer->PrepareFrame(window_width, window_height)) {
        AbandonFrame();
        return false;
    }

    for (auto& [id, element] : m_elements) element->PrepareFrame(this);
    m_renderer->ReleaseRetiredTextures();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));

    ImGui::Begin(m_windowId, nullptr, m_window_flags);

    if (!m_surfaceQuarantined) RenderElements();

    // *** DEBUG ***
    if (m_debug && !m_surfaceQuarantined) {
        if (m_debugFocusRequested) ImGui::SetNextWindowFocus();
        m_debugFocusRequested = false;
        RenderDebugWindow();
    }
    // *** END DEBUG ***

    ImGui::End();
    ImGui::Render();
    m_renderer->FinishFrame();
    if (m_diagnosticsEnabled.load(std::memory_order_relaxed)) {
        m_pendingDiagnosticsFrame = m_surfaceQuarantined ? json{{"surfaceStatus", "quarantined"}} : BuildDiagnosticsStateUnlocked();
        m_pendingDiagnosticsFrame["constructedAtMs"] = DiagnosticsNowMs();
        m_pendingDiagnosticsFrame["vertices"] = ImGui::GetDrawData()->TotalVtxCount;
    } else if (!m_pendingDiagnosticsFrame.is_null()) {
        // Discard a frame whose backend could not submit before diagnostics stopped.
        m_pendingDiagnosticsFrame = nullptr;
    }
    return true;
};

void XFrames::RenderDebugWindow() {
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
void XFrames::ExtractNumberFromStyleDef(const json& styleDef, const char* key, T& value) {
    if (styleDef.contains(key) && styleDef[key].is_number_unsigned()) {
        value = styleDef[key].template get<T>();
    }
};

void XFrames::ExtractBooleanFromStyleDef(const json& styleDef, const char* key, bool& value) {
    if (styleDef.contains(key) && styleDef[key].is_number_unsigned()) {
        value = styleDef[key].template get<bool>();
    }
};

void XFrames::ExtractImVec2FromStyleDef(const json& styleDef, const char* key, ImVec2& value) {
    if (styleDef.contains(key)  && styleDef[key].is_array() && styleDef[key].size() == 2) {
        value.x = styleDef[key][0].template get<float>();
        value.y = styleDef[key][1].template get<float>();
    }
};

void XFrames::PatchStyle(const json& styleDef) {
    if (styleDef.is_object()) {
        const std::lock_guard<std::mutex> hierarchyLock(m_hierarchy_mutex);
        const std::lock_guard<std::mutex> elementsLock(m_elements_mutex);
        if (m_runtimeDisposed) return;

        auto nextStyle = ImGui::GetStyle();
        ImGuiStyle* style = &nextStyle;

        ExtractNumberFromStyleDef<float>(styleDef, "alpha", style->Alpha);
        ExtractNumberFromStyleDef<float>(styleDef, "disabledAlpha", style->DisabledAlpha);
        ExtractImVec2FromStyleDef(styleDef, "windowPadding", style->WindowPadding);
        ExtractNumberFromStyleDef<float>(styleDef, "windowRounding", style->WindowRounding);
        ExtractNumberFromStyleDef<float>(styleDef, "windowBorderSize", style->WindowBorderSize);
        ExtractImVec2FromStyleDef(styleDef, "windowMinSize", style->WindowMinSize);
        ExtractImVec2FromStyleDef(styleDef, "windowTitleAlign", style->WindowTitleAlign);
        if (styleDef.contains("windowMenuButtonPosition") && styleDef["windowMenuButtonPosition"].is_number()) {
            style->WindowMenuButtonPosition = static_cast<ImGuiDir>(styleDef["windowMenuButtonPosition"].template get<int>());
        }
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
        ExtractNumberFromStyleDef<float>(styleDef, "tabMinWidthForCloseButton", style->TabCloseButtonMinWidthUnselected);
        ExtractNumberFromStyleDef<float>(styleDef, "tabBarBorderSize", style->TabBarBorderSize);
        ExtractNumberFromStyleDef<float>(styleDef, "tableAngledHeadersAngle", style->TableAngledHeadersAngle);
        ExtractImVec2FromStyleDef(styleDef, "tableAngledHeadersTextAlign", style->TableAngledHeadersTextAlign);
        if (styleDef.contains("colorButtonPosition") && styleDef["colorButtonPosition"].is_number()) {
            style->ColorButtonPosition = static_cast<ImGuiDir>(styleDef["colorButtonPosition"].template get<int>());
        }
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
        ImGui::GetStyle() = nextStyle;
        TakeStyleSnapshot();
        m_frameScheduler.Invalidate(xframes::FrameReason::Style);
    }
    if (styleDef.is_object()) m_frameScheduler.Notify();
};

void XFrames::TakeStyleSnapshot() {
    const auto style = ImGui::GetStyle();

    // This is necessary as the style is repeatedly modified during render via push and pop calls
    memcpy(&m_appStyle, &style, sizeof(style));
};

void XFrames::QueueElementInternalOp(const int id, std::string& widgetOpDef) {
    const std::lock_guard<std::mutex> dispatchLock(m_commitMutex);
    if (m_surfaceQuarantined) return;
    try {
        const json opDef = json::parse(widgetOpDef);

        // Copy ownership under the element lock, then release it before delivery:
        // the subject handler takes that lock and verifies its lifetime owner.
        std::optional<rpp::subjects::serialized_replay_subject<json>> subject;
        {
            const std::lock_guard<std::mutex> lock(m_elements_mutex);
            const auto it = m_elementInternalOpsSubject.find(id);
            if (it != m_elementInternalOpsSubject.end()) subject = it->second;
        }
        if (subject) {
            subject->get_observer().on_next(opDef);
            m_frameScheduler.Notify();
        }
    } catch (nlohmann::detail::parse_error& parseError) {
        printf("XFrames::QueueElementInternalOp, parse error: %s\n", parseError.what());
    }
};

void XFrames::PatchElementUnlocked(const json& patchDef) {
    const auto id = patchDef.at("id").get<int>();
    if (auto it = m_elements.find(id); it != m_elements.end()) it->second->Patch(patchDef, this);
}

bool XFrames::IsElementAlive(const int id) {
    const std::lock_guard<std::mutex> lock(m_elements_mutex);
    return !m_surfaceQuarantined && m_elements.contains(id);
}

void XFrames::DestroyElementUnlocked(const int id, std::vector<int>* destroyedIds) {
    m_hierarchy.erase(id);
    // Clean up per-widget reactive subject
    m_elementInternalOpsSubject.erase(id);
    m_diagnosticsLastInternalOpMs.erase(id);

    // Erase from element registry — triggers unique_ptr destructor chain:
    // LayoutNode::~LayoutNode frees YGNode,
    // MapView::~MapView frees GPU tile textures,
    // Image::~Image frees GPU texture
    if (m_elements.erase(id) && destroyedIds) destroyedIds->push_back(id);
}

std::vector<int> XFrames::GetChildren(int id) {
    const std::lock_guard<std::mutex> lock(m_hierarchy_mutex);
    if (m_surfaceQuarantined) throw xframes::CommitError("surface_quarantined", "Publication failed; recreate the native runtime");
    const auto it = m_hierarchy.find(id);
    return it == m_hierarchy.end() ? std::vector<int>{} : it->second;
};

float XFrames::GetChildrenMaxBottom(int parentId) const {
    float maxBottom = 0;
    if (m_hierarchy.contains(parentId)) {
        for (const auto& childId : m_hierarchy.at(parentId)) {
            if (m_elements.contains(childId)) {
                auto* node = m_elements.at(childId)->m_layoutNode->m_node;
                float bottom = YGNodeLayoutGetTop(node) + YGNodeLayoutGetHeight(node);
                if (bottom > maxBottom) maxBottom = bottom;
            }
        }
    }
    return maxBottom;
}

void XFrames::InvalidateMaxBottomCaches() {
    for (auto& [id, el] : m_elements) {
        if (el->m_cull) {
            el->m_maxBottomDirty = true;
        }
    }
}

// todo: switch to ReactivePlusPlus's BehaviorSubject
void XFrames::AppendTextToClippedMultiLineTextRenderer(const int id, const std::string& data) {
    const std::lock_guard<std::mutex> dispatchLock(m_commitMutex);
    if (m_surfaceQuarantined) return;
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(m_elements_mutex);
        if (m_elements.contains(id) && m_elements[id]->m_type == "clipped-multi-line-text-renderer") {
            dynamic_cast<ClippedMultiLineTextRenderer*>(m_elements[id].get())->AppendText(data.c_str());
            m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
            changed = true;
        }
    }
    if (changed) m_frameScheduler.Notify();
};

StyleVarValueRef XFrames::GetStyleVar(const ImGuiStyleVar key) {
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
ImFont* XFrames::GetWidgetFont(const StyledWidget* widget) {
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
float XFrames::GetWidgetFontSize(const StyledWidget* widget) {
    if (widget->HasCustomStyles() && widget->HasCustomFont(this)) {
        // auto result = widget->m_style.value()->GetCustomFontId(widget->GetState(), this);

        // if (result.has_value()) {
            return m_renderer->m_loadedFonts[widget->m_style.value()->GetCustomFontId(widget->GetState(), this)]->LegacySize;
        // }
    }

    // return 16.0f;

    ImGuiIO& io = m_renderer->m_imGuiCtx->IO;

    if (!io.FontDefault || !io.FontDefault->LegacySize) {
        return 16.0f;
    }

    // Return default font size as we might be in the middle of rendering a widget with a custom font
    return io.FontDefault->LegacySize;
}

float XFrames::GetTextLineHeight(const StyledWidget* widget) {
    return GetWidgetFontSize(widget);
};

float XFrames::GetTextLineHeightWithSpacing(const StyledWidget* widget) {
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

float XFrames::GetFrameHeight(const StyledWidget* widget) {
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

float XFrames::GetFrameHeightWithSpacing(const StyledWidget* widget) {
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

ImVec2 XFrames::CalcTextSize(const StyledWidget* widget, const char* text, const char* text_end, bool hide_text_after_double_hash, float wrap_width)
{
    auto font = GetWidgetFont(widget);
    const char* text_display_end;

    if (hide_text_after_double_hash) {
        text_display_end = ImGui::FindRenderedTextEnd(text, text_end);      // Hide anything after a '##' string
    } else {
        text_display_end = text_end;
    }

    const float font_size = font->LegacySize;
    if (text == text_display_end)
        return ImVec2(0.0f, font_size);
    ImVec2 text_size = font->CalcTextSizeA(font->LegacySize, FLT_MAX, wrap_width, text, text_display_end, NULL);

    // Round
    // FIXME: This has been here since Dec 2015 (7b0bf230) but down the line we want this out.
    // FIXME: Investigate using ceilf or e.g.
    // - https://git.musl-libc.org/cgit/musl/tree/src/math/ceilf.c
    // - https://embarkstudios.github.io/rust-gpu/api/src/libm/math/ceilf.rs.html
    text_size.x = IM_TRUNC(text_size.x + 0.99999f);

    return text_size;
}
