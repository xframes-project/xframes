#include <algorithm>
#include <imgui.h>
#include <thread>
#include <yoga/YGNodeLayout.h>

#ifndef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include "stb_image.h"
#endif

#include "shared.h"
#include "tiledownloader.h"
#include "widget/map_view.h"
#include "xframes.h"
#include "imgui_renderer.h"

// Clamp tile rect to viewport, adjust UVs to match the visible portion.
// Returns false if the tile is entirely outside the viewport.
static bool ClipTileToViewport(
    const ImVec2& viewP0, const ImVec2& viewP1,
    ImVec2& tileP0, ImVec2& tileP1,
    ImVec2& uvP0, ImVec2& uvP1)
{
    float tileW = tileP1.x - tileP0.x;
    float tileH = tileP1.y - tileP0.y;
    if (tileW <= 0 || tileH <= 0) return false;

    float clippedX0 = std::max(tileP0.x, viewP0.x);
    float clippedY0 = std::max(tileP0.y, viewP0.y);
    float clippedX1 = std::min(tileP1.x, viewP1.x);
    float clippedY1 = std::min(tileP1.y, viewP1.y);

    if (clippedX0 >= clippedX1 || clippedY0 >= clippedY1) return false;

    float uvW = uvP1.x - uvP0.x;
    float uvH = uvP1.y - uvP0.y;

    uvP1.x = uvP0.x + uvW * (clippedX1 - tileP0.x) / tileW;
    uvP1.y = uvP0.y + uvH * (clippedY1 - tileP0.y) / tileH;
    uvP0.x = uvP0.x + uvW * (clippedX0 - tileP0.x) / tileW;
    uvP0.y = uvP0.y + uvH * (clippedY0 - tileP0.y) / tileH;

    tileP0 = ImVec2(clippedX0, clippedY0);
    tileP1 = ImVec2(clippedX1, clippedY1);
    return true;
}

bool MapView::HasCustomWidth() {
    return false;
}

bool MapView::HasCustomHeight() {
    return false;
}

std::string MapView::BuildTileUrl(int x, int y, int zoom) {
    return replaceTokens(m_tileUrlTemplate, [&](const std::string& token) -> std::optional<std::string> {
        if (token == "z") return std::to_string(zoom);
        if (token == "x") return std::to_string(x);
        if (token == "y") return std::to_string(y);
        return std::nullopt;
    });
}

void MapView::FetchMissingTiles(int xMin, int xMax, int yMin, int yMax) {
    int maxTiles = 1 << m_zoom;

    std::vector<TileKey> toFetch;

    {
        std::lock_guard<std::mutex> lock(m_inflightMutex);
        for (int x = xMin; x < xMax; x++) {
            for (int y = yMin; y < yMax; y++) {
                if (y < 0 || y >= maxTiles) continue;

                int wrappedX = ((x % maxTiles) + maxTiles) % maxTiles;
                TileKey key{wrappedX, y, m_zoom};

                if (m_tileTextures.contains(key)) continue;
                if (m_inflightKeys.contains(key)) continue;

                m_inflightKeys.insert(key);
                toFetch.push_back(key);
            }
        }
    }

    if (toFetch.empty()) return;

#ifndef __EMSCRIPTEN__
    auto headers = m_tileRequestHeaders;
    auto tileUrlTemplate = m_tileUrlTemplate;
    int zoom = m_zoom;
    bool diskCacheEnabled = m_diskCache.isEnabled();

    std::thread([this, toFetch = std::move(toFetch), headers, tileUrlTemplate, zoom, diskCacheEnabled]() {
        auto& cache = TileCache::getGlobalInstance();

        for (const auto& key : toFetch) {
            std::string url = replaceTokens(tileUrlTemplate, [&](const std::string& token) -> std::optional<std::string> {
                if (token == "z") return std::to_string(key.zoom);
                if (token == "x") return std::to_string(key.x);
                if (token == "y") return std::to_string(key.y);
                return std::nullopt;
            });

            std::vector<unsigned char> pngData;

            // Three-tier cache: memory → disk → network
            auto cached = cache.get(url);
            if (cached) {
                pngData = std::move(*cached);
                m_cacheStats.memoryHits++;
            } else if (diskCacheEnabled) {
                auto diskCached = m_diskCache.get(key.x, key.y, key.zoom);
                if (diskCached) {
                    pngData = std::move(*diskCached);
                    cache.put(url, pngData.data(), pngData.size());
                    m_cacheStats.diskHits++;
                }
            }

            if (pngData.empty()) {
                fetchTile(url, headers, [&](bool success, std::vector<uint8_t> data) {
                    if (success) {
                        pngData.assign(data.begin(), data.end());
                    }
                });
                if (!pngData.empty()) {
                    cache.put(url, pngData.data(), pngData.size());
                    if (diskCacheEnabled) {
                        m_diskCache.put(key.x, key.y, key.zoom, pngData.data(), pngData.size());
                    }
                    m_cacheStats.networkFetches++;
                }
            }

            if (!pngData.empty()) {
                std::lock_guard<std::mutex> lock(m_pendingMutex);
                m_pendingTiles.push_back(PendingTile{key, std::move(pngData)});
            }

            // Remove from inflight regardless of success
            {
                std::lock_guard<std::mutex> lock(m_inflightMutex);
                m_inflightKeys.erase(key);
            }
        }
    }).detach();
#endif
}

void MapView::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    // Upload pending tiles to GPU (render thread)
#ifndef __EMSCRIPTEN__
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        for (auto& pending : m_pendingTiles) {
            // Skip if we already have this tile (e.g. from a duplicate fetch)
            if (m_tileTextures.contains(pending.key)) continue;

            int w = 0, h = 0;
            unsigned char* pixels = stbi_load_from_memory(
                pending.pngData.data(),
                static_cast<int>(pending.pngData.size()),
                &w, &h, nullptr, 4
            );

            if (pixels) {
                GLuint texId = 0;
                glGenTextures(1, &texId);
                glBindTexture(GL_TEXTURE_2D, texId);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

                stbi_image_free(pixels);

                if (glGetError() == GL_NO_ERROR) {
                    // Evict LRU tiles if over budget
                    while (m_tileTextures.size() >= MAX_GPU_TILES) {
                        auto& oldestKey = m_textureLruOrder.back();
                        auto evictIt = m_tileTextures.find(oldestKey);
                        if (evictIt != m_tileTextures.end()) {
                            glDeleteTextures(1, &evictIt->second.first.textureView);
                            m_tileTextures.erase(evictIt);
                        }
                        m_textureLruOrder.pop_back();
                    }

                    Texture tex;
                    tex.textureView = texId;
                    tex.width = w;
                    tex.height = h;
                    m_textureLruOrder.push_front(pending.key);
                    m_tileTextures[pending.key] = { tex, m_textureLruOrder.begin() };
                } else {
                    glDeleteTextures(1, &texId);
                }
            }
        }
        m_pendingTiles.clear();
    }
#endif

    if (!m_initialized) return;

    float viewW = YGNodeLayoutGetWidth(m_layoutNode->m_node);
    float viewH = YGNodeLayoutGetHeight(m_layoutNode->m_node);

    if (viewW <= 0 || viewH <= 0) return;

    ImGui::PushID(m_id);
    ImGui::BeginGroup();

    ImGui::InvisibleButton("##map_canvas", ImVec2(viewW, viewH));
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

    const ImVec2 p0 = ImGui::GetItemRectMin();

    bool isDragging = ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);

    if (isDragging) {
        float dx = ImGui::GetIO().MouseDelta.x;
        float dy = ImGui::GetIO().MouseDelta.y;

        // Track pan direction for prefetching (center moves opposite to drag)
        if (dx > 0.5f) m_panDirX = -1;
        else if (dx < -0.5f) m_panDirX = 1;
        if (dy > 0.5f) m_panDirY = -1;
        else if (dy < -0.5f) m_panDirY = 1;

        // Convert pixel delta to tile coordinate delta
        m_centerTileX -= static_cast<double>(dx) / TILE_SIZE;
        m_centerTileY -= static_cast<double>(dy) / TILE_SIZE;

        // Update lon/lat from tile coords
        m_centerLon = xToLon(m_centerTileX, m_zoom);
        m_centerLat = yToLat(m_centerTileY, m_zoom);

        m_wasDragging = true;
    }

    if (m_wasDragging && !isDragging) {
        m_wasDragging = false;
        m_panDirX = 0;
        m_panDirY = 0;
    }

    // Double-click to zoom in (centered on click point)
    if (ImGui::IsItemHovered() && !m_wasDragging && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        int newZoom = std::clamp(m_zoom + 1, m_minZoom, m_maxZoom);
        if (newZoom != m_zoom) {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            float mx = mousePos.x - p0.x;
            float my = mousePos.y - p0.y;

            double mouseTileX = m_centerTileX + (mx - viewW / 2.0) / TILE_SIZE;
            double mouseTileY = m_centerTileY + (my - viewH / 2.0) / TILE_SIZE;

            double mouseLon = xToLon(mouseTileX, m_zoom);
            double mouseLat = yToLat(mouseTileY, m_zoom);

            m_zoom = newZoom;
            m_lastZoomChangeTime = std::chrono::steady_clock::now();
            m_zoomDebouncing = true;

            double newMouseTileX = lonToX(mouseLon, m_zoom);
            double newMouseTileY = latToY(mouseLat, m_zoom);

            m_centerTileX = newMouseTileX - (mx - viewW / 2.0) / TILE_SIZE;
            m_centerTileY = newMouseTileY - (my - viewH / 2.0) / TILE_SIZE;

            m_centerLon = xToLon(m_centerTileX, m_zoom);
            m_centerLat = yToLat(m_centerTileY, m_zoom);

            view->m_onNumericValueChange(m_id, static_cast<float>(m_zoom));
        }
    }

    // Mouse wheel zoom (centered on cursor)
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            int newZoom = std::clamp(m_zoom + static_cast<int>(wheel), m_minZoom, m_maxZoom);
            if (newZoom != m_zoom) {
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                float mx = mousePos.x - p0.x;
                float my = mousePos.y - p0.y;

                // Mouse position in tile coords at current zoom
                double mouseTileX = m_centerTileX + (mx - viewW / 2.0) / TILE_SIZE;
                double mouseTileY = m_centerTileY + (my - viewH / 2.0) / TILE_SIZE;

                // Convert to lon/lat (zoom-independent)
                double mouseLon = xToLon(mouseTileX, m_zoom);
                double mouseLat = yToLat(mouseTileY, m_zoom);

                m_zoom = newZoom;
                m_lastZoomChangeTime = std::chrono::steady_clock::now();
                m_zoomDebouncing = true;

                // Recompute at new zoom
                double newMouseTileX = lonToX(mouseLon, m_zoom);
                double newMouseTileY = latToY(mouseLat, m_zoom);

                // Adjust center so cursor stays on same geo location
                m_centerTileX = newMouseTileX - (mx - viewW / 2.0) / TILE_SIZE;
                m_centerTileY = newMouseTileY - (my - viewH / 2.0) / TILE_SIZE;

                m_centerLon = xToLon(m_centerTileX, m_zoom);
                m_centerLat = yToLat(m_centerTileY, m_zoom);

                view->m_onNumericValueChange(m_id, static_cast<float>(m_zoom));
            }
        }
    }

    if (!ImGui::IsItemVisible()) {
        ImGui::EndGroup();
        ImGui::PopID();
        return;
    }

    const ImVec2 p1 = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImGui::PushClipRect(p0, p1, true);

    // Compute visible tile range
    int xMin = static_cast<int>(floor(m_centerTileX - (viewW / 2.0) / TILE_SIZE));
    int xMax = static_cast<int>(ceil(m_centerTileX + (viewW / 2.0) / TILE_SIZE));
    int yMin = static_cast<int>(floor(m_centerTileY - (viewH / 2.0) / TILE_SIZE));
    int yMax = static_cast<int>(ceil(m_centerTileY + (viewH / 2.0) / TILE_SIZE));

    int maxTiles = 1 << m_zoom;

    // Render old-zoom tiles as scaled placeholders (background layer)
    for (auto& [key, entry] : m_tileTextures) {
        if (key.zoom == m_zoom) continue;

        double scale = pow(2.0, m_zoom - key.zoom);
        double tileWorldSize = TILE_SIZE * scale;

        double tileX = key.x * scale;
        double tileY = key.y * scale;

        float px = static_cast<float>(round((tileX - m_centerTileX) * TILE_SIZE + viewW / 2.0));
        float py = static_cast<float>(round((tileY - m_centerTileY) * TILE_SIZE + viewH / 2.0));

        ImVec2 tileP0(p0.x + px, p0.y + py);
        ImVec2 tileP1(p0.x + px + static_cast<float>(tileWorldSize),
                      p0.y + py + static_cast<float>(tileWorldSize));

        ImVec2 uv0(0, 0), uv1(1, 1);
        if (ClipTileToViewport(p0, p1, tileP0, tileP1, uv0, uv1)) {
            // Promote to front of LRU
            m_textureLruOrder.splice(m_textureLruOrder.begin(), m_textureLruOrder, entry.second);
            drawList->AddImage((void*)(intptr_t)entry.first.textureView, tileP0, tileP1, uv0, uv1);
        }
    }

    bool allCurrentTilesLoaded = true;

    for (int x = xMin; x < xMax; x++) {
        for (int y = yMin; y < yMax; y++) {
            if (y < 0 || y >= maxTiles) continue;

            int wrappedX = ((x % maxTiles) + maxTiles) % maxTiles;
            TileKey key{wrappedX, y, m_zoom};

            // Screen position of this tile
            float px = static_cast<float>(round((x - m_centerTileX) * TILE_SIZE + viewW / 2.0));
            float py = static_cast<float>(round((y - m_centerTileY) * TILE_SIZE + viewH / 2.0));

            ImVec2 tileP0(p0.x + px, p0.y + py);
            ImVec2 tileP1(p0.x + px + TILE_SIZE, p0.y + py + TILE_SIZE);

            auto it = m_tileTextures.find(key);
            if (it != m_tileTextures.end()) {
                // Promote to front of LRU
                m_textureLruOrder.splice(m_textureLruOrder.begin(), m_textureLruOrder, it->second.second);
                ImVec2 uv0(0, 0), uv1(1, 1);
                if (ClipTileToViewport(p0, p1, tileP0, tileP1, uv0, uv1)) {
                    drawList->AddImage(
                        (void*)(intptr_t)it->second.first.textureView,
                        tileP0, tileP1, uv0, uv1
                    );
                }
            } else {
                // Placeholder: gray rect (clamp to viewport)
                ImVec2 clampedP0(std::max(tileP0.x, p0.x), std::max(tileP0.y, p0.y));
                ImVec2 clampedP1(std::min(tileP1.x, p1.x), std::min(tileP1.y, p1.y));
                if (clampedP0.x < clampedP1.x && clampedP0.y < clampedP1.y) {
                    drawList->AddRectFilled(clampedP0, clampedP1, IM_COL32(200, 200, 200, 255));
                }
                allCurrentTilesLoaded = false;
            }
        }
    }

    // GPU texture eviction: keep current-zoom nearby tiles + old-zoom tiles overlapping viewport
#ifndef __EMSCRIPTEN__
    {
        std::set<TileKey> nearbyKeys;
        for (int x = xMin - 2; x < xMax + 2; x++) {
            for (int y = std::max(0, yMin - 2); y < std::min(maxTiles, yMax + 2); y++) {
                int wrappedX = ((x % maxTiles) + maxTiles) % maxTiles;
                nearbyKeys.insert(TileKey{wrappedX, y, m_zoom});
            }
        }

        // Only keep old-zoom tiles if current zoom has missing tiles
        if (!allCurrentTilesLoaded) {
            for (const auto& [key, entry] : m_tileTextures) {
                if (key.zoom == m_zoom) continue;
                double scale = pow(2.0, m_zoom - key.zoom);
                double tileX = key.x * scale;
                double tileY = key.y * scale;
                if (tileX + scale > xMin && tileX < xMax &&
                    tileY + scale > yMin && tileY < yMax) {
                    nearbyKeys.insert(key);
                }
            }
        }

        std::vector<TileKey> toEvict;
        for (const auto& [key, entry] : m_tileTextures) {
            if (!nearbyKeys.count(key)) {
                toEvict.push_back(key);
            }
        }

        for (const auto& key : toEvict) {
            auto it = m_tileTextures.find(key);
            if (it != m_tileTextures.end()) {
                glDeleteTextures(1, &it->second.first.textureView);
                m_textureLruOrder.erase(it->second.second);
                m_tileTextures.erase(it);
            }
        }
    }
#endif

    // Expand fetch range in pan direction for prefetching
    int fetchXMin = xMin + std::min(m_panDirX, 0);
    int fetchXMax = xMax + std::max(m_panDirX, 0);
    int fetchYMin = yMin + std::min(m_panDirY, 0);
    int fetchYMax = yMax + std::max(m_panDirY, 0);

    // Debounce tile fetches during rapid zoom (150ms after last scroll)
    if (m_zoomDebouncing) {
        auto elapsed = std::chrono::steady_clock::now() - m_lastZoomChangeTime;
        if (elapsed >= std::chrono::milliseconds(150)) {
            m_zoomDebouncing = false;
            FetchMissingTiles(fetchXMin, fetchXMax, fetchYMin, fetchYMax);
        }
    } else {
        FetchMissingTiles(fetchXMin, fetchXMax, fetchYMin, fetchYMax);
    }

    // Attribution overlay
    if (!m_attribution.empty()) {
        ImFont* font = ImGui::GetIO().FontDefault;
        float fontSize = font->LegacySize;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, m_attribution.c_str());

        float pad = 4.0f;
        ImVec2 boxP1(p1.x - 2.0f, p1.y - 2.0f);
        ImVec2 boxP0(boxP1.x - textSize.x - pad * 2, boxP1.y - textSize.y - pad * 2);

        drawList->AddRectFilled(boxP0, boxP1, IM_COL32(255, 255, 255, 180), 2.0f);
        drawList->AddText(font, fontSize, ImVec2(boxP0.x + pad, boxP0.y + pad),
                          IM_COL32(0, 0, 0, 200), m_attribution.c_str());
    }

    // Loading indicator
    if (!allCurrentTilesLoaded) {
        const char* loadingText = "Loading...";
        ImFont* font = ImGui::GetIO().FontDefault;
        float fontSize = font->LegacySize;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, loadingText);

        float pad = 4.0f;
        ImVec2 boxP0(p0.x + 2.0f, p0.y + 2.0f);
        ImVec2 boxP1(boxP0.x + textSize.x + pad * 2, boxP0.y + textSize.y + pad * 2);

        drawList->AddRectFilled(boxP0, boxP1, IM_COL32(0, 0, 0, 160), 2.0f);
        drawList->AddText(font, fontSize, ImVec2(boxP0.x + pad, boxP0.y + pad),
                          IM_COL32(255, 255, 255, 220), loadingText);
    }

    // Cache stats overlay (top-right, only when disk cache is enabled)
    if (m_diskCache.isEnabled()) {
        std::string statsText = "GPU: " + std::to_string(m_tileTextures.size()) + "/" + std::to_string(MAX_GPU_TILES) +
                                " Mem: " + std::to_string(m_cacheStats.memoryHits) +
                                " Disk: " + std::to_string(m_cacheStats.diskHits) +
                                " Net: " + std::to_string(m_cacheStats.networkFetches);

        if (m_prefetching.load()) {
            statsText += "  Prefetching " + std::to_string(m_prefetchCompleted.load()) +
                         "/" + std::to_string(m_prefetchTotal.load());
        }

        ImFont* font = ImGui::GetIO().FontDefault;
        float fontSize = font->LegacySize;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, statsText.c_str());

        float pad = 4.0f;
        ImVec2 boxP1(p1.x - 2.0f, p0.y + textSize.y + pad * 2 + 2.0f);
        ImVec2 boxP0(boxP1.x - textSize.x - pad * 2, p0.y + 2.0f);

        drawList->AddRectFilled(boxP0, boxP1, IM_COL32(0, 0, 0, 160), 2.0f);
        drawList->AddText(font, fontSize, ImVec2(boxP0.x + pad, boxP0.y + pad),
                          IM_COL32(255, 255, 255, 220), statsText.c_str());
    }

    // Render polylines
    for (const auto& polyline : m_polylines) {
        if (polyline.points.size() < 2) continue;

        std::vector<ImVec2> screenPoints;
        screenPoints.reserve(polyline.points.size());

        for (const auto& [lat, lon] : polyline.points) {
            double tileX = lonToX(lon, m_zoom);
            double tileY = latToY(lat, m_zoom);
            float px = static_cast<float>(round((tileX - m_centerTileX) * TILE_SIZE + viewW / 2.0));
            float py = static_cast<float>(round((tileY - m_centerTileY) * TILE_SIZE + viewH / 2.0));
            screenPoints.emplace_back(p0.x + px, p0.y + py);
        }

        ImU32 col = ImColor(polyline.color);
        drawList->AddPolyline(screenPoints.data(), static_cast<int>(screenPoints.size()),
                              col, ImDrawFlags_None, polyline.thickness);
    }

    // Render accuracy overlays (circles / ellipses)
    for (const auto& overlay : m_overlays) {
        double tileX = lonToX(overlay.lon, m_zoom);
        double tileY = latToY(overlay.lat, m_zoom);
        float px = static_cast<float>(round((tileX - m_centerTileX) * TILE_SIZE + viewW / 2.0));
        float py = static_cast<float>(round((tileY - m_centerTileY) * TILE_SIZE + viewH / 2.0));
        ImVec2 center(p0.x + px, p0.y + py);

        float majorPx = static_cast<float>(meterToPixel(overlay.radiusMeters, m_zoom, overlay.lat));
        if (majorPx < 1.0f) continue;

        ImU32 fillCol = ImColor(overlay.fillColor);
        ImU32 strokeCol = ImColor(overlay.strokeColor);

        if (overlay.radiusMinorMeters <= 0.0) {
            drawList->AddCircleFilled(center, majorPx, fillCol);
            drawList->AddCircle(center, majorPx, strokeCol, 0, overlay.strokeThickness);
        } else {
            float minorPx = static_cast<float>(meterToPixel(overlay.radiusMinorMeters, m_zoom, overlay.lat));
            float rotRad = overlay.rotation * (static_cast<float>(M_PI) / 180.0f);
            drawList->AddEllipseFilled(center, ImVec2(majorPx, minorPx), fillCol, rotRad);
            drawList->AddEllipse(center, ImVec2(majorPx, minorPx), strokeCol, rotRad, 0, overlay.strokeThickness);
        }
    }

    // Render pin markers
    for (const auto& marker : m_markers) {
        double markerTileX = lonToX(marker.lon, m_zoom);
        double markerTileY = latToY(marker.lat, m_zoom);

        float px = static_cast<float>(round((markerTileX - m_centerTileX) * TILE_SIZE + viewW / 2.0));
        float py = static_cast<float>(round((markerTileY - m_centerTileY) * TILE_SIZE + viewH / 2.0));

        ImVec2 screenPos(p0.x + px, p0.y + py);

        // Skip if outside viewport (with radius margin)
        if (screenPos.x + marker.radius < p0.x || screenPos.x - marker.radius > p1.x ||
            screenPos.y + marker.radius < p0.y || screenPos.y - marker.radius > p1.y) continue;

        ImU32 col = ImColor(marker.color);
        drawList->AddCircleFilled(screenPos, marker.radius, col);
        drawList->AddCircle(screenPos, marker.radius, IM_COL32(0, 0, 0, 180), 0, 1.5f);

        if (!marker.label.empty()) {
            ImFont* font = ImGui::GetIO().FontDefault;
            float fontSize = font->LegacySize;
            ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, marker.label.c_str());
            ImVec2 textPos(screenPos.x - textSize.x / 2, screenPos.y - marker.radius - textSize.y - 2);
            drawList->AddRectFilled(
                ImVec2(textPos.x - 2, textPos.y - 1),
                ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 1),
                IM_COL32(0, 0, 0, 160), 2.0f);
            drawList->AddText(font, fontSize, textPos, IM_COL32(255, 255, 255, 240), marker.label.c_str());
        }
    }

    // Coordinate overlay (bottom-left, only when hovered)
    if (ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        float mx = mousePos.x - p0.x;
        float my = mousePos.y - p0.y;

        double mouseTileX = m_centerTileX + (mx - viewW / 2.0) / TILE_SIZE;
        double mouseTileY = m_centerTileY + (my - viewH / 2.0) / TILE_SIZE;
        double mouseLon = xToLon(mouseTileX, m_zoom);
        double mouseLat = yToLat(mouseTileY, m_zoom);

        char coordText[64];
        snprintf(coordText, sizeof(coordText), "%.4f, %.4f", mouseLat, mouseLon);

        ImFont* font = ImGui::GetIO().FontDefault;
        float fontSize = font->LegacySize;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, coordText);

        float pad = 4.0f;
        ImVec2 boxP0(p0.x + 2.0f, p1.y - textSize.y - pad * 2 - 2.0f);
        ImVec2 boxP1(boxP0.x + textSize.x + pad * 2, p1.y - 2.0f);

        drawList->AddRectFilled(boxP0, boxP1, IM_COL32(0, 0, 0, 160), 2.0f);
        drawList->AddText(font, fontSize, ImVec2(boxP0.x + pad, boxP0.y + pad),
                          IM_COL32(255, 255, 255, 220), coordText);
    }

    ImGui::PopClipRect();
    ImGui::EndGroup();
    ImGui::PopID();
}

void MapView::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("tileUrlTemplate") && widgetPatchDef["tileUrlTemplate"].is_string()) {
        m_tileUrlTemplate = widgetPatchDef["tileUrlTemplate"].template get<std::string>();
#ifndef __EMSCRIPTEN__
        for (auto& [key, entry] : m_tileTextures) {
            glDeleteTextures(1, &entry.first.textureView);
        }
#endif
        m_tileTextures.clear();
        m_textureLruOrder.clear();
    }
    if (widgetPatchDef.contains("attribution") && widgetPatchDef["attribution"].is_string()) {
        m_attribution = widgetPatchDef["attribution"].template get<std::string>();
    }
    if (widgetPatchDef.contains("tileRequestHeaders") && widgetPatchDef["tileRequestHeaders"].is_object()) {
        m_tileRequestHeaders.clear();
        for (auto& [key, val] : widgetPatchDef["tileRequestHeaders"].items()) {
            if (val.is_string()) {
                m_tileRequestHeaders[key] = val.template get<std::string>();
            }
        }
    }
    if (widgetPatchDef.contains("minZoom") && widgetPatchDef["minZoom"].is_number_integer()) {
        m_minZoom = widgetPatchDef["minZoom"].template get<int>();
    }
    if (widgetPatchDef.contains("maxZoom") && widgetPatchDef["maxZoom"].is_number_integer()) {
        m_maxZoom = widgetPatchDef["maxZoom"].template get<int>();
    }
    if (widgetPatchDef.contains("cachePath") && widgetPatchDef["cachePath"].is_string()) {
        auto newPath = widgetPatchDef["cachePath"].template get<std::string>();
        if (newPath != m_cachePath) {
            m_cachePath = newPath;
            m_diskCache.configure(m_cachePath);
        }
    }
}

bool MapView::HasInternalOps() {
    return true;
}

void MapView::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        auto op = opDef["op"].template get<std::string>();

        if (op == "render"
            && opDef.contains("centerX") && opDef["centerX"].is_number()
            && opDef.contains("centerY") && opDef["centerY"].is_number()
            && opDef.contains("zoom") && opDef["zoom"].is_number()) {

            auto lon = opDef["centerX"].template get<double>();
            auto lat = opDef["centerY"].template get<double>();
            auto zoom = opDef["zoom"].template get<int>();

            m_centerLon = lon;
            m_centerLat = lat;
            m_zoom = std::clamp(zoom, m_minZoom, m_maxZoom);
            m_centerTileX = lonToX(lon, zoom);
            m_centerTileY = latToY(lat, zoom);

            if (!m_initialized) {
                TileCache::getGlobalInstance().configure(1024, 3600000);
            }
            m_initialized = true;

            // Compute visible tile range
            float viewW = YGNodeLayoutGetWidth(m_layoutNode->m_node);
            float viewH = YGNodeLayoutGetHeight(m_layoutNode->m_node);
            if (viewW <= 0) viewW = 600;
            if (viewH <= 0) viewH = 600;

            int xMin = static_cast<int>(floor(m_centerTileX - (viewW / 2.0) / TILE_SIZE));
            int xMax = static_cast<int>(ceil(m_centerTileX + (viewW / 2.0) / TILE_SIZE));
            int yMin = static_cast<int>(floor(m_centerTileY - (viewH / 2.0) / TILE_SIZE));
            int yMax = static_cast<int>(ceil(m_centerTileY + (viewH / 2.0) / TILE_SIZE));

            FetchMissingTiles(xMin, xMax, yMin, yMax);
        // WARNING: Bulk tile downloading violates the usage policy of OpenStreetMap's
        // default tile servers (tile.openstreetmap.org). Only use this with a tile server
        // that permits it. See: https://operations.osmfoundation.org/policies/tiles/
        } else if (op == "prefetch"
            && opDef.contains("minLon") && opDef["minLon"].is_number()
            && opDef.contains("minLat") && opDef["minLat"].is_number()
            && opDef.contains("maxLon") && opDef["maxLon"].is_number()
            && opDef.contains("maxLat") && opDef["maxLat"].is_number()
            && opDef.contains("minZoom") && opDef["minZoom"].is_number_integer()
            && opDef.contains("maxZoom") && opDef["maxZoom"].is_number_integer()) {

            if (!m_diskCache.isEnabled()) return;
            if (m_prefetching.load()) return;

            auto minLon = opDef["minLon"].template get<double>();
            auto minLat = opDef["minLat"].template get<double>();
            auto maxLon = opDef["maxLon"].template get<double>();
            auto maxLat = opDef["maxLat"].template get<double>();
            auto prefetchMinZoom = opDef["minZoom"].template get<int>();
            auto prefetchMaxZoom = opDef["maxZoom"].template get<int>();

            prefetchMinZoom = std::clamp(prefetchMinZoom, m_minZoom, m_maxZoom);
            prefetchMaxZoom = std::clamp(prefetchMaxZoom, m_minZoom, m_maxZoom);
            if (prefetchMinZoom > prefetchMaxZoom) return;

            // Enumerate all tiles across zoom levels
            struct TileRange { int x, y, zoom; };
            std::vector<TileRange> tilesToFetch;

            for (int z = prefetchMinZoom; z <= prefetchMaxZoom; z++) {
                int xMin = static_cast<int>(floor(lonToX(minLon, z)));
                int xMax = static_cast<int>(floor(lonToX(maxLon, z)));
                int yMin = static_cast<int>(floor(latToY(maxLat, z)));  // lat is inverted
                int yMax = static_cast<int>(floor(latToY(minLat, z)));

                int maxTile = 1 << z;
                for (int x = xMin; x <= xMax; x++) {
                    for (int y = yMin; y <= yMax; y++) {
                        if (y < 0 || y >= maxTile) continue;
                        int wrappedX = ((x % maxTile) + maxTile) % maxTile;
                        // Skip tiles already on disk
                        if (m_diskCache.get(wrappedX, y, z).has_value()) continue;
                        tilesToFetch.push_back({wrappedX, y, z});
                    }
                }
            }

            int total = static_cast<int>(tilesToFetch.size());
            if (total == 0) {
                // All tiles already cached, fire completion
                m_view->m_onPrefetchProgress(m_id, 0, 0);
                return;
            }

            m_prefetching = true;
            m_prefetchCompleted = 0;
            m_prefetchTotal = total;

#ifndef __EMSCRIPTEN__
            auto headers = m_tileRequestHeaders;
            auto tileUrlTemplate = m_tileUrlTemplate;
            auto onProgress = m_view->m_onPrefetchProgress;
            int widgetId = m_id;

            std::thread([this, tilesToFetch = std::move(tilesToFetch), headers, tileUrlTemplate, onProgress, widgetId, total]() {
                auto& cache = TileCache::getGlobalInstance();
                int completed = 0;

                for (const auto& tile : tilesToFetch) {
                    std::string url = replaceTokens(tileUrlTemplate, [&](const std::string& token) -> std::optional<std::string> {
                        if (token == "z") return std::to_string(tile.zoom);
                        if (token == "x") return std::to_string(tile.x);
                        if (token == "y") return std::to_string(tile.y);
                        return std::nullopt;
                    });

                    std::vector<unsigned char> pngData;

                    // Check memory cache first
                    auto cached = cache.get(url);
                    if (cached) {
                        pngData = std::move(*cached);
                    }

                    if (pngData.empty()) {
                        fetchTile(url, headers, [&](bool success, std::vector<uint8_t> data) {
                            if (success) {
                                pngData.assign(data.begin(), data.end());
                            }
                        });
                    }

                    if (!pngData.empty()) {
                        cache.put(url, pngData.data(), pngData.size());
                        m_diskCache.put(tile.x, tile.y, tile.zoom, pngData.data(), pngData.size());
                        m_cacheStats.networkFetches++;
                    }

                    completed++;
                    m_prefetchCompleted = completed;
                    onProgress(widgetId, completed, total);
                }

                m_prefetching = false;
            }).detach();
#endif
        } else if (op == "setMarkers" && opDef.contains("markers") && opDef["markers"].is_array()) {
            m_markers.clear();
            for (auto& [key, item] : opDef["markers"].items()) {
                if (item.is_object() && item.contains("lat") && item.contains("lon")) {
                    MapMarker m;
                    m.lat = item["lat"].template get<double>();
                    m.lon = item["lon"].template get<double>();
                    if (item.contains("color")) {
                        auto c = extractColor(item["color"]);
                        if (c.has_value()) m.color = c.value();
                    }
                    if (item.contains("label") && item["label"].is_string()) {
                        m.label = item["label"].template get<std::string>();
                    }
                    if (item.contains("radius") && item["radius"].is_number()) {
                        m.radius = item["radius"].template get<float>();
                    }
                    m_markers.push_back(m);
                }
            }
        } else if (op == "clearMarkers") {
            m_markers.clear();
        } else if (op == "setPolylines" && opDef.contains("polylines") && opDef["polylines"].is_array()) {
            m_polylines.clear();
            for (auto& [key, item] : opDef["polylines"].items()) {
                if (item.is_object() && item.contains("points") && item["points"].is_array()) {
                    MapPolyline pl;
                    for (auto& [pk, pt] : item["points"].items()) {
                        if (pt.is_object() && pt.contains("lat") && pt.contains("lon")) {
                            pl.points.emplace_back(pt["lat"].get<double>(), pt["lon"].get<double>());
                        }
                    }
                    if (item.contains("color")) {
                        auto c = extractColor(item["color"]);
                        if (c.has_value()) pl.color = c.value();
                    }
                    if (item.contains("thickness") && item["thickness"].is_number()) {
                        pl.thickness = item["thickness"].get<float>();
                    }
                    if (item.contains("pointsLimit") && item["pointsLimit"].is_number_integer()) {
                        pl.pointsLimit = item["pointsLimit"].get<size_t>();
                    }
                    m_polylines.push_back(std::move(pl));
                }
            }
        } else if (op == "clearPolylines") {
            m_polylines.clear();
        } else if (op == "setOverlays" && opDef.contains("overlays") && opDef["overlays"].is_array()) {
            m_overlays.clear();
            for (auto& [key, item] : opDef["overlays"].items()) {
                if (item.is_object() && item.contains("lat") && item.contains("lon") && item.contains("radiusMeters")) {
                    MapOverlay o;
                    o.lat = item["lat"].get<double>();
                    o.lon = item["lon"].get<double>();
                    o.radiusMeters = item["radiusMeters"].get<double>();
                    if (item.contains("radiusMinorMeters") && item["radiusMinorMeters"].is_number())
                        o.radiusMinorMeters = item["radiusMinorMeters"].get<double>();
                    if (item.contains("rotation") && item["rotation"].is_number())
                        o.rotation = item["rotation"].get<float>();
                    if (item.contains("fillColor")) {
                        auto c = extractColor(item["fillColor"]);
                        if (c.has_value()) o.fillColor = c.value();
                    }
                    if (item.contains("strokeColor")) {
                        auto c = extractColor(item["strokeColor"]);
                        if (c.has_value()) o.strokeColor = c.value();
                    }
                    if (item.contains("strokeThickness") && item["strokeThickness"].is_number())
                        o.strokeThickness = item["strokeThickness"].get<float>();
                    m_overlays.push_back(o);
                }
            }
        } else if (op == "clearOverlays") {
            m_overlays.clear();
        } else if (op == "appendPolylinePoint"
            && opDef.contains("polylineIndex") && opDef["polylineIndex"].is_number_integer()
            && opDef.contains("lat") && opDef["lat"].is_number()
            && opDef.contains("lon") && opDef["lon"].is_number()) {
            auto idx = opDef["polylineIndex"].get<int>();
            if (idx >= 0 && idx < static_cast<int>(m_polylines.size())) {
                auto& pl = m_polylines[idx];
                if (pl.pointsLimit > 0 && pl.points.size() >= pl.pointsLimit) {
                    pl.points.erase(pl.points.begin());
                }
                pl.points.emplace_back(opDef["lat"].get<double>(), opDef["lon"].get<double>());
            }
        }
    }
}
