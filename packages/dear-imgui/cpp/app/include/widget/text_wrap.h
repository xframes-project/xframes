#include <nlohmann/json.hpp>

#include "styled_widget.h"

class TextWrap final : public StyledWidget {
public:
    float m_width;

    static std::unique_ptr<TextWrap> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, ReactImgui* view) {
        auto id = widgetDef["id"].template get<int>();
        double width = widgetDef["width"].template get<double>();

        return std::make_unique<TextWrap>(view, id, width, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
    }

    TextWrap(ReactImgui* view, int id, const float& width, std::optional<WidgetStyle>& style);

    void Render(ReactImgui* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, ReactImgui* view) override;
};