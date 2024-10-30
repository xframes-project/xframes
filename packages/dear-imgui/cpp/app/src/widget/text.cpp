#include <nlohmann/json.hpp>

#include "widget/text.h"
#include "widget/styled_widget.h"

void Text::Patch(const json& widgetPatchDef, ReactImgui* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("text") && widgetPatchDef["text"].is_string()) {
        m_text = widgetPatchDef["text"].template get<std::string>();
    }
};

std::unique_ptr<BulletText> BulletText::makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, ReactImgui* view) {
    auto id = widgetDef["id"].template get<int>();
    auto text = widgetDef["text"].template get<std::string>();

    return std::make_unique<BulletText>(view, id, text, maybeStyle);

    // throw std::invalid_argument("Invalid JSON data");
};

void BulletText::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImGui::BulletText("%s", m_text.c_str());
};

std::unique_ptr<UnformattedText> UnformattedText::makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, ReactImgui* view) {
    auto id = widgetDef["id"].template get<int>();
    auto text = widgetDef["text"].template get<std::string>();

    return std::make_unique<UnformattedText>(view, id, text, maybeStyle);

    // throw std::invalid_argument("Invalid JSON data");
};

void UnformattedText::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImGui::TextUnformatted(m_text.c_str());
};

std::unique_ptr<DisabledText> DisabledText::makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, ReactImgui* view) {
    auto id = widgetDef["id"].template get<int>();
    auto text = widgetDef["text"].template get<std::string>();

    return std::make_unique<DisabledText>(view, id, text, maybeStyle);

    // throw std::invalid_argument("Invalid JSON data");
};

void DisabledText::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImGui::TextDisabled("%s", m_text.c_str());
};