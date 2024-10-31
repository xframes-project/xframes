#ifndef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

#include <queue>
#include <string>
#include <mutex>
#include <rpp/rpp.hpp>
#include "imgui.h"
#include "imgui_internal.h"
#include <nlohmann/json.hpp>
#include "yoga/YGEnums.h"

#include "shared.h"
#include "imgui_helpers.h"
#include "texture_helpers.h"

using json = nlohmann::json;

#pragma once

class ImGuiRenderer;
class Widget;
class MapGenerator;
class Element;
class StyledWidget;
class LayoutNode;
struct WidgetStyle;

enum ElementOp {
    OpCreateElement,
    OpPatchElement,
    OpSetChildren,
    OpAppendChild,
};

struct ElementOpDef {
    ElementOp op;
    json data;
};

class XFrames {
    private:
        std::optional<std::string> m_rawStyleOverridesDefs;

        const char* m_windowId;

        ImGuiWindowFlags m_window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

        std::unordered_map<int, rpp::subjects::serialized_replay_subject<json>> m_elementInternalOpsSubject;

        rpp::subjects::serialized_replay_subject<ElementOpDef> m_elementOpSubject;

        std::unordered_map<std::string, std::function<std::unique_ptr<Element>(const json&, std::optional<WidgetStyle>, XFrames*)>> m_element_init_fn;

        std::unordered_map<int, std::unique_ptr<Element>> m_elements;
        std::mutex m_elements_mutex;

        bool m_debug;

        void CreateElement(const json& elementDef);

        void PatchElement(const json& patchDef);

        void HandleElementInternalOp(const json& opDef);

        void SetChildren(const json& opDef);

        void AppendChild(const json& opDef);
        
        void SetUpFloatFormatChars();

        void SetUpElementCreatorFunctions();

    public:
        ImGuiRenderer* m_renderer;
        std::unordered_map<int, std::vector<int>> m_hierarchy;
        std::mutex m_hierarchy_mutex;

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

        XFrames(const char* newWindowId, std::optional<std::string> rawStyleOverridesDefs);

        void Init(ImGuiRenderer* renderer);

        void SetDebug(bool debug);

        void ShowDebugWindow();

        void RenderElementById(int id, const std::optional<ImRect>& viewport = std::nullopt);

        void SetUpSubjects();

        void SetEventHandlers(
            OnInitCallback onInitFn,
            OnTextChangedCallback onInputTextChangeFn,
            OnComboChangedCallback onComboChangeFn,
            OnNumericValueChangedCallback onNumericValueChangeFn,
            OnMultipleNumericValuesChangedCallback onMultiValueChangeFn,
            OnBooleanValueChangedCallback onBooleanValueChangeFn,
            OnClickCallback onClickFn
        );

        void PrepareForRender();

        void RenderDebugWindow();

        void Render(int window_width, int window_height);

        void SetChildrenDisplay(int id, YGDisplay display);

        void RenderChildren(int id, const std::optional<ImRect>& viewport = std::nullopt);

        void RenderElementTree(int id = 0);

        void RenderElements(int id = 0, const std::optional<ImRect>& viewport = std::nullopt);

        void QueueCreateElement(std::string& elementJsonAsString);

        void QueuePatchElement(int id, std::string& elementJsonAsString);

        void QueueSetChildren(int id, const std::vector<int>& childIds);

        void QueueAppendChild(int parentId, int childId);

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
