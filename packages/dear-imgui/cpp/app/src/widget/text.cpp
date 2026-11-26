#include <nlohmann/json.hpp>

#include "widget/text.h"
#include "widget/styled_widget.h"

void Text::TrackWrapWidth(XFrames* view) {
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float wrapPosX = window->DC.TextWrapPos;
    const float wrapWidth = wrapPosX >= 0.0f ? ImGui::CalcWrapWidthForPos(window->DC.CursorPos, wrapPosX) : 0.0f;
    if (wrapWidth != m_wrapWidth) {
        m_wrapWidth = wrapWidth;
        YGNodeMarkDirty(m_layoutNode->m_node);
        view->m_frameScheduler.Invalidate(xframes::FrameReason::Layout);
    }
}

void Text::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("text") && widgetPatchDef["text"].is_string()) {
        m_text = widgetPatchDef["text"].template get<std::string>();
    }
};

std::unique_ptr<BulletText> BulletText::makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
    auto id = widgetDef["id"].template get<int>();
    auto text = widgetDef["text"].template get<std::string>();

    return std::make_unique<BulletText>(view, id, text, maybeStyle);

    // throw std::invalid_argument("Invalid JSON data");
};

void BulletText::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::BulletText("%s", m_text.c_str());
};

std::unique_ptr<UnformattedText> UnformattedText::makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
    auto id = widgetDef["id"].template get<int>();
    auto text = widgetDef["text"].template get<std::string>();

    return std::make_unique<UnformattedText>(view, id, text, maybeStyle);

    // throw std::invalid_argument("Invalid JSON data");
};

void UnformattedText::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    TrackWrapWidth(view);
    ImGui::TextUnformatted(m_text.c_str());
};

std::unique_ptr<DisabledText> DisabledText::makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
    auto id = widgetDef["id"].template get<int>();
    auto text = widgetDef["text"].template get<std::string>();

    return std::make_unique<DisabledText>(view, id, text, maybeStyle);

    // throw std::invalid_argument("Invalid JSON data");
};

void DisabledText::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    TrackWrapWidth(view);
    ImGui::TextDisabled("%s", m_text.c_str());
};