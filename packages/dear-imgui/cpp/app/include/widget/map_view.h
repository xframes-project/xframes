#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <set>
#include <vector>
#include "shared.h"
#include "tiledownloader.h"
#include "tilecache.h"
#include "disk_tile_cache.h"
#include "styled_widget.h"
#include "texture_helpers.h"

struct TileKey {
    int x, y, zoom;

    bool operator==(const TileKey& other) const {
        return x == other.x && y == other.y && zoom == other.zoom;
    }

    bool operator<(const TileKey& other) const {
        if (zoom != other.zoom) return zoom < other.zoom;
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
};

struct TileKeyHash {
    size_t operator()(const TileKey& k) const {
        size_t h = std::hash<int>{}(k.x);
        h ^= std::hash<int>{}(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.zoom) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

class MapView final : public StyledWidget {
private:
    // Map state
    double m_centerTileX = 0.0;
    double m_centerTileY = 0.0;
    double m_centerLon = 0.0;
    double m_centerLat = 0.0;
    int m_zoom = 1;
    int m_minZoom = 1;
    int m_maxZoom = 17;
    bool m_wasDragging = false;
    bool m_initialized = false;

    static constexpr int TILE_SIZE = 256;
    std::string m_tileUrlTemplate = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
    std::unordered_map<std::string, std::string> m_tileRequestHeaders;
    std::string m_attribution = "\xC2\xA9 OpenStreetMap contributors";

    // GPU texture registry with LRU eviction (render thread only)
    static constexpr int MAX_GPU_TILES = 512;
    std::list<TileKey> m_textureLruOrder;  // most recent at front
    std::unordered_map<TileKey, std::pair<Texture, std::list<TileKey>::iterator>, TileKeyHash> m_tileTextures;

    // Pending tiles from background download thread
    struct PendingTile {
        TileKey key;
        std::vector<unsigned char> pngData;
    };
    std::mutex m_pendingMutex;
    std::vector<PendingTile> m_pendingTiles;

    // Zoom debounce
    std::chrono::steady_clock::time_point m_lastZoomChangeTime{};
    bool m_zoomDebouncing = false;

    // Track in-flight downloads to avoid duplicates
    std::mutex m_inflightMutex;
    std::set<TileKey> m_inflightKeys;

    // Disk tile cache + stats
    DiskTileCache m_diskCache;
    TileCacheStats m_cacheStats;
    std::string m_cachePath;

    // Prefetch state
    std::atomic<bool> m_prefetching{false};
    std::atomic<int> m_prefetchCompleted{0};
    std::atomic<int> m_prefetchTotal{0};

    // Helpers
    void FetchMissingTiles(int xMin, int xMax, int yMin, int yMax);
    std::string BuildTileUrl(int x, int y, int zoom);

public:
    static std::unique_ptr<MapView> makeWidget(const json& widgetDef, std::optional<WidgetStyle> maybeStyle, XFrames* view) {
        auto id = widgetDef["id"].template get<int>();
        auto widget = std::make_unique<MapView>(view, id, maybeStyle);

        if (widgetDef.contains("tileUrlTemplate") && widgetDef["tileUrlTemplate"].is_string()) {
            widget->m_tileUrlTemplate = widgetDef["tileUrlTemplate"].template get<std::string>();
        }
        if (widgetDef.contains("attribution") && widgetDef["attribution"].is_string()) {
            widget->m_attribution = widgetDef["attribution"].template get<std::string>();
        }
        if (widgetDef.contains("tileRequestHeaders") && widgetDef["tileRequestHeaders"].is_object()) {
            for (auto& [key, val] : widgetDef["tileRequestHeaders"].items()) {
                if (val.is_string()) {
                    widget->m_tileRequestHeaders[key] = val.template get<std::string>();
                }
            }
        }

        if (widgetDef.contains("minZoom") && widgetDef["minZoom"].is_number_integer()) {
            widget->m_minZoom = widgetDef["minZoom"].template get<int>();
        }
        if (widgetDef.contains("maxZoom") && widgetDef["maxZoom"].is_number_integer()) {
            widget->m_maxZoom = widgetDef["maxZoom"].template get<int>();
        }
        if (widgetDef.contains("cachePath") && widgetDef["cachePath"].is_string()) {
            widget->m_cachePath = widgetDef["cachePath"].template get<std::string>();
            widget->m_diskCache.configure(widget->m_cachePath);
        }

        return widget;
    }

    bool HasCustomWidth() override;
    bool HasCustomHeight() override;

    MapView(XFrames* view, const int id, std::optional<WidgetStyle>& style) : StyledWidget(view, id, style) {
        m_type = "map-view";
        m_tileRequestHeaders["User-Agent"] = "xframes/1.0";
    }

    void Render(XFrames* view, const std::optional<ImRect>& viewport) override;
    void Patch(const json& widgetPatchDef, XFrames* view) override;
    bool HasInternalOps() override;
    void HandleInternalOp(const json& opDef) override;
};
