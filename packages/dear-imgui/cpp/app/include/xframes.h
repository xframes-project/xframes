#ifndef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

#include <queue>
#include <string>
#include <mutex>
#include <atomic>
#include <memory>
#include <new>
#include <rpp/rpp.hpp>
#include "imgui.h"
#include "imgui_internal.h"
#include <nlohmann/json.hpp>
#include "yoga/YGEnums.h"

#include "shared.h"
#include "imgui_helpers.h"
#include "texture_helpers.h"
#include "commit.h"

using json = nlohmann::json;

#pragma once

class ImGuiRenderer;
class Widget;
class MapGenerator;
class Element;
class StyledWidget;
class LayoutNode;
struct WidgetStyle;

struct CommitRequest {
    xframes::CommitBatch batch;
    xframes::CommitResult result;
    std::exception_ptr exception;
    bool completed = false;
};

class XFrames {
    private:
        friend class XFramesTest;
        friend class Element;
        friend class Widget;

        std::optional<std::string> m_rawStyleOverridesDefs;

        const char* m_windowId;

        ImGuiWindowFlags m_window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

        std::unordered_map<int, rpp::subjects::serialized_replay_subject<json>> m_elementInternalOpsSubject;

        // Lock order: structural dispatch -> subject -> hierarchy -> elements.
        // Rendering takes hierarchy -> elements. Publication holds both for the
        // entire preflight/application/revision boundary.
        std::mutex m_commitMutex;
        uint64_t m_nativeSequence = 0;
        uint64_t m_nativeRevision = 0;
        std::unordered_set<int> m_publicationOwnedIds;
        std::atomic<bool> m_surfaceQuarantined{false};
        bool m_subjectsReady = false;
        rpp::subjects::serialized_replay_subject<std::weak_ptr<CommitRequest>> m_elementOpSubject;
        rpp::composite_disposable_wrapper m_commitSubscription = rpp::composite_disposable_wrapper::make();
        json m_lastCommitDiagnostics;
        xframes::CommitResult DispatchCommit(xframes::CommitBatch batch);
        xframes::CommitResult ApplyCommitOperations(xframes::CommitBatch& batch);

        std::unordered_map<std::string, std::function<std::unique_ptr<Element>(const json&, std::optional<WidgetStyle>, XFrames*)>> m_element_init_fn;

        std::unordered_map<int, std::unique_ptr<Element>> m_elements;
        std::mutex m_elements_mutex;
        std::unordered_map<int, std::vector<int>> m_hierarchy;
        std::mutex m_hierarchy_mutex;

        bool m_debug;

        std::atomic<bool> m_diagnosticsEnabled{false};
        std::mutex m_diagnosticsMutex;
        json m_diagnosticsFrame = {{"enabled", false}, {"frame", 0}};
        json m_pendingDiagnosticsFrame;
        uint64_t m_diagnosticsFrameCount = 0; // render-thread owned
        std::unordered_map<int, double> m_diagnosticsLastInternalOpMs; // element-mutex owned
        json BuildDiagnosticsStateUnlocked();

        void CreateElementUnlocked(const json& elementDef);

        void PatchElementUnlocked(const json& patchDef);



        void DestroyElementUnlocked(int id, std::vector<int>* destroyedIds);
        
        void SetUpFloatFormatChars();

        void SetUpElementCreatorFunctions();

        // Render traversal helpers require Render's hierarchy/element locks.
        // Widget/Element call them while constructing that same frame.
        void RenderElementById(int id, const std::optional<ImRect>& viewport = std::nullopt);
        void RenderDebugWindow();
        void SetChildrenDisplay(int id, YGDisplay display);
        void RenderChildren(int id, const std::optional<ImRect>& viewport = std::nullopt);
        void RenderElementTree(int id = 0);
        void RenderElements(int id = 0, const std::optional<ImRect>& viewport = std::nullopt);
        float GetChildrenMaxBottom(int parentId) const;
        void InvalidateMaxBottomCaches();

    public:
        ImGuiRenderer* m_renderer;

#ifndef __EMSCRIPTEN__
        std::unordered_map<int, GLuint> m_imageToTextureMap;
#endif

        std::queue<ImageJob> m_imageJobs;

        std::unordered_map<int, std::unique_ptr<char[]>> m_floatFormatChars;

        ImGuiStyle m_appStyle;

        OnInitCallback m_onInit;
        OnTextChangedCallback m_onInputTextChange;
        OnComboChangedCallback m_onComboChange;
        OnNumericValueChangedCallback m_onNumericValueChange;
        OnMultipleNumericValuesChangedCallback m_onMultiValueChange;
        OnBooleanValueChangedCallback m_onBooleanValueChange;
        OnClickCallback m_onClick;
        OnTableSortCallback m_onTableSort;
        OnTableFilterCallback m_onTableFilter;
        OnTableRowClickCallback m_onTableRowClick;
        OnTableItemActionCallback m_onTableItemAction;
        OnPrefetchProgressCallback m_onPrefetchProgress;
        OnScriptErrorCallback m_onScriptError;

        XFrames(const char* newWindowId, std::optional<std::string> rawStyleOverridesDefs);
        ~XFrames();

        void Init(ImGuiRenderer* renderer);

        void SetDebug(bool debug);

        // Observational test API. Frame snapshots are copied from the render thread.
        void SetDiagnosticsEnabled(bool enabled);
        json GetDiagnosticsFrame();
        json GetDiagnosticsState(); // CPU-only snapshot for native tests
        void CompleteDiagnosticsFrame();
        static double DiagnosticsNowMs();

        void ShowDebugWindow();

        void SetUpSubjects();

        void SetEventHandlers(
            OnInitCallback onInitFn,
            OnTextChangedCallback onInputTextChangeFn,
            OnComboChangedCallback onComboChangeFn,
            OnNumericValueChangedCallback onNumericValueChangeFn,
            OnMultipleNumericValuesChangedCallback onMultiValueChangeFn,
            OnBooleanValueChangedCallback onBooleanValueChangeFn,
            OnClickCallback onClickFn,
            OnTableSortCallback onTableSortFn,
            OnTableFilterCallback onTableFilterFn,
            OnTableRowClickCallback onTableRowClickFn,
            OnTableItemActionCallback onTableItemActionFn,
            OnPrefetchProgressCallback onPrefetchProgressFn,
            OnScriptErrorCallback onScriptErrorFn
        );

        void PrepareForRender();

        void Render(int window_width, int window_height);


        // Synchronous structural API. uint64 counters use decimal strings on the wire.
        xframes::CommitResult ApplyCommit(std::string_view serializedCommit);
        json GetCommitState();



        bool IsElementAlive(int id);


        void QueueElementInternalOp(int id, std::string& widgetOpDef);

        void AppendTextToClippedMultiLineTextRenderer(int id, const std::string& data);

        std::vector<int> GetChildren(int id);

        json GetAvailableFonts();

        template <typename T>
        void ExtractNumberFromStyleDef(const json& styleDef, const char* key, T& value);

        void ExtractBooleanFromStyleDef(const json& styleDef, const char* key, bool& value);

        void ExtractImVec2FromStyleDef(const json& styleDef, const char* key, ImVec2& value);

        void PatchStyle(const json& styleDef);

        StyleVarValueRef GetStyleVar(ImGuiStyleVar key);

        ImVec2 CalcTextSize(const StyledWidget* widget, const char* text, const char* text_end = nullptr, bool hide_text_after_double_hash = false, float wrap_width = -1.0f);

        ImFont* GetWidgetFont(const StyledWidget* widget);

        float GetWidgetFontSize(const StyledWidget* widget);

        float GetTextLineHeight(const StyledWidget* widget);

        float GetTextLineHeightWithSpacing(const StyledWidget* widget);

        float GetFrameHeight(const StyledWidget* widget);

        float GetFrameHeightWithSpacing(const StyledWidget* widget);

        void TakeStyleSnapshot();
};

template <typename T, typename std::enable_if<std::is_base_of<Widget, T>::value, int>::type = 0>
std::unique_ptr<T> makeWidget(const json& val, std::optional<WidgetStyle> maybeStyle, XFrames* view);

std::unique_ptr<Element> makeElement(const json& val, XFrames* view);
