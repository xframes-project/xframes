#include <optional>
#include <widget/widget.h>

#pragma once

class XFrames;

struct WidgetStyleParts {
    std::optional<StyleColors> maybeColors;
    std::optional<StyleVars> maybeStyleVars;
    std::optional<int> maybeFontIndex;
};

struct WidgetStyle {
    std::optional<WidgetStyleParts> maybeBase;
    std::optional<WidgetStyleParts> maybeDisabled;
    std::optional<WidgetStyleParts> maybeHover;
    std::optional<WidgetStyleParts> maybeActive;

    bool HasCustomFont(std::optional<ElementState> widgetState, XFrames* view);
    [[nodiscard]] bool HasCustomStyleVar(std::optional<ElementState> widgetState, ImGuiStyleVar key) const;
    [[nodiscard]] bool HasCustomColors(std::optional<ElementState> widgetState) const;
    [[nodiscard]] bool HasCustomStyleVars(std::optional<ElementState> widgetState) const;
    StyleColors& GetCustomColors(std::optional<ElementState> widgetState);
    StyleVars& GetCustomStyleVars(std::optional<ElementState> widgetState);
    int GetCustomFontId(std::optional<ElementState> widgetState, XFrames* view);
    StyleVarValue GetCustomStyleVar(std::optional<ElementState> widgetState, ImGuiStyleVar key);
};

WidgetStyleParts extractStyleParts(const json& styleDef, XFrames* view);

class StyledWidget : public Widget {
public:
    std::optional<std::unique_ptr<WidgetStyle>> m_style;

    static std::optional<WidgetStyle> ExtractStyle(const json& widgetDef, XFrames* view);

    explicit StyledWidget(XFrames* view, int id);

    StyledWidget(XFrames* view, int id, std::optional<WidgetStyle>& maybeStyle);

    void PreRender(XFrames* view) override;

    void PostRender(XFrames* view) override;

    [[nodiscard]] bool HasCustomStyles() const;

    bool HasCustomFont(XFrames* view) const;

    [[nodiscard]] bool HasCustomColors() const;

    virtual bool HasCustomWidth();

    virtual bool HasCustomHeight();

    [[nodiscard]] bool HasCustomStyleVars() const;

    void ReplaceStyle(WidgetStyle& newStyle);

    void Patch(const json& widgetPatchDef, XFrames* view) override;

    [[nodiscard]] bool HasCustomStyleVar(ImGuiStyleVar key) const;

    [[nodiscard]] StyleVarValue GetCustomStyleVar(ImGuiStyleVar key) const;
};