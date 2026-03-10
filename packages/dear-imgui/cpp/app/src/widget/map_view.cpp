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

    std::thread([this, toFetch = std::move(toFetch), headers, tileUrlTemplate, zoom]() {
        auto& cache = TileCache::getGlobalInstance();

        for (const auto& key : toFetch) {
            std::string url = replaceTokens(tileUrlTemplate, [&](const std::string& token) -> std::optional<std::string> {
                if (token == "z") return std::to_string(key.zoom);
                if (token == "x") return std::to_string(key.x);
                if (token == "y") return std::to_string(key.y);
                return std::nullopt;
            });

            std::vector<unsigned char> pngData;

            // Check cache first
            auto cached = cache.get(url);
            if (cached) {
                pngData = std::move(*cached);
            } else {
                fetchTile(url, headers, [&](bool success, std::vector<uint8_t> data) {
                    if (success) {
                        pngData.assign(data.begin(), data.end());
                    }
                });
                if (!pngData.empty()) {
                    cache.put(url, pngData.data(), pngData.size());
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
                    Texture tex;
                    tex.textureView = texId;
                    tex.width = w;
                    tex.height = h;
                    m_tileTextures[pending.key] = tex;
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
    }

    // Mouse wheel zoom (centered on cursor)
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            int newZoom = std::clamp(m_zoom + static_cast<int>(wheel), 1, 17);
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
    for (const auto& [key, tex] : m_tileTextures) {
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

        drawList->AddImage((void*)(intptr_t)tex.textureView, tileP0, tileP1,
                           ImVec2(0, 0), ImVec2(1, 1));
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
                drawList->AddImage(
                    (void*)(intptr_t)it->second.textureView,
                    tileP0, tileP1,
                    ImVec2(0, 0), ImVec2(1, 1)
                );
            } else {
                // Placeholder: gray rect
                drawList->AddRectFilled(tileP0, tileP1, IM_COL32(200, 200, 200, 255));
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
            for (const auto& [key, tex] : m_tileTextures) {
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
        for (const auto& [key, tex] : m_tileTextures) {
            if (!nearbyKeys.count(key)) {
                toEvict.push_back(key);
            }
        }

        for (const auto& key : toEvict) {
            auto it = m_tileTextures.find(key);
            if (it != m_tileTextures.end()) {
                glDeleteTextures(1, &it->second.textureView);
                m_tileTextures.erase(it);
            }
        }
    }
#endif

    // Always fetch missing visible tiles (handles drag, zoom, initial render)
    FetchMissingTiles(xMin, xMax, yMin, yMax);

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

    ImGui::PopClipRect();
    ImGui::EndGroup();
    ImGui::PopID();
}

void MapView::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("tileUrlTemplate") && widgetPatchDef["tileUrlTemplate"].is_string()) {
        m_tileUrlTemplate = widgetPatchDef["tileUrlTemplate"].template get<std::string>();
#ifndef __EMSCRIPTEN__
        for (auto& [key, tex] : m_tileTextures) {
            glDeleteTextures(1, &tex.textureView);
        }
#endif
        m_tileTextures.clear();
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
            m_zoom = zoom;
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
        }
    }
}
