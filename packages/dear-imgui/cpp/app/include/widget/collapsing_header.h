#include "styled_widget.h"

class CollapsingHeader final : public StyledWidget {
public:
    std::string m_label;

    static std::unique_ptr<CollapsingHeader> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, ReactImgui* view) {
        auto id = widgetDef["id"].template get<int>();
        const auto label = widgetDef["label"].template get<std::string>();

        return std::make_unique<CollapsingHeader>(view, id, label, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
    }

    CollapsingHeader(ReactImgui* view, int id, const std::string& label, std::optional<WidgetStyle>& style);

    void Render(ReactImgui* view, const std::optional<ImRect>& viewport) override;

    void Patch(const json& widgetPatchDef, ReactImgui* view) override;

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;
};