#include <mutex>
#include <vector>
#include "mapgenerator.h"
#include "styled_widget.h"
#include "texture_helpers.h"

class MapView final : public StyledWidget {
private:
    ImVec2 m_offset;

    double m_centerLon = 0.0;
    double m_centerLat = 0.0;
    int m_zoom = 1;
    bool m_wasDragging = false;

    static constexpr int BUFFER_MULTIPLIER = 3;

    int m_mapGeneratorJobCounter = 0;
    std::unordered_map<int, std::unique_ptr<MapGenerator>> m_mapGeneratorJobs;

    std::unordered_map<int, std::unique_ptr<Texture>> m_textures;

#ifndef __EMSCRIPTEN__
    struct PendingTexture {
        std::vector<unsigned char> data;
        int width;
        int height;
        ImVec2 offset;
    };
    std::mutex m_pendingMutex;
    std::optional<PendingTexture> m_pendingTexture;
#endif

public:
    static std::unique_ptr<MapView> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();

        return std::make_unique<MapView>(view, id, maybeStyle);

        // throw std::invalid_argument("Invalid JSON data");
    }

    bool HasCustomWidth() override;

    bool HasCustomHeight() override;

    MapView(XFrames* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
        m_type = "map-view";

        m_offset = ImVec2(0.0f, 0.0f);
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

    bool HasInternalOps() override;

    void HandleInternalOp(const json& opDef) override;
};
