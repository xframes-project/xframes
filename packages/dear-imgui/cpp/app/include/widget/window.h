#include "styled_widget.h"

class Window final : public StyledWidget {
public:
    ImGuiWindowFlags m_flags;
    bool m_open;
    std::string m_title;
    float m_width;
    float m_height;

    static std::unique_ptr<Window> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        auto title = widgetDef.contains("title") ? widgetDef["title"].template get<std::string>() : "";
        auto width = widgetDef.contains("width") ? widgetDef["width"].template get<float>() : 720.0f;
        auto height = widgetDef.contains("height") ? widgetDef["height"].template get<float>() : 480.0f;

        return std::make_unique<Window>(view, id, title, width, height, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
    }

    Window(XFrames* view, int id, const std::string& title, float width, float height, std::optional<WidgetStyle>& style);

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, XFrames* view) override;
};
