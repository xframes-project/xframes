#include "styled_widget.h"

class Group final : public StyledWidget {
public:
    static std::unique_ptr<Group> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();

        return std::make_unique<Group>(view, id, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
    }

    Group(XFrames* view, int id, std::optional<WidgetStyle>& style);

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
};
