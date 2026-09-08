#include <algorithm>
#include <imgui.h>
#include <climits>
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

MapView::MapView(XFrames* view, int id, std::optional<WidgetStyle>& style)
    : StyledWidget(view, id, style) {
    m_type = "map-view";
#ifndef __EMSCRIPTEN__
    m_tileRequestHeaders["User-Agent"] = "xframes/1.0";
#endif
    ResetRequests();
}

void MapView::CloseRequests() {
    if (!m_async) return;
    {
        const std::lock_guard lock(m_async->mutex);
        m_async->alive = false;
        m_async->pending.clear();
        m_async->inflight.clear();
        m_resourceLifetime.Reset();
    }
#ifdef __EMSCRIPTEN__
    m_fetches.CancelAll();
#else
    m_view->GetMapWorker().Cancel(m_async);
#endif
    m_prefetchQueue.clear();
}

void MapView::ResetRequests() {
    CloseRequests();
    m_resourceLifetime = m_view->m_frameScheduler.Register(xframes::FrameReason::Map);
    m_async = std::make_shared<AsyncState>();
    m_async->source = m_resourceLifetime.GetSource();
    if (!m_cachePath.empty()) m_async->disk->configure(m_cachePath);
    m_requestedKeys.clear();
    m_cacheStats = {};
    m_prefetching = false;
    m_prefetchCompleted = m_prefetchTotal = 0;
}

MapView::~MapView() {
    CloseRequests();
    for (auto& [key, entry] : m_tileTextures)
        if (m_view->m_renderer) m_view->m_renderer->RetireTexture(entry.first);
}

void MapView::CompleteTile(std::weak_ptr<AsyncState> weak, TileKey key, bool prefetch,
                           std::vector<unsigned char> bytes, int cacheTier) {
    auto state = weak.lock();
    if (!state) return;
    {
        const std::lock_guard lock(state->mutex);
        if (!state->alive) return;
        state->lastLoadFailed = bytes.empty();
        if (!bytes.empty()) {
            if (cacheTier == 1) ++state->stats.memoryHits;
            else if (cacheTier == 2) ++state->stats.diskHits;
            else ++state->stats.networkFetches;
        }
        if (prefetch) {
            --state->prefetchInflight;
            ++state->prefetchCompleted;
            state->progressPending = true;
        } else {
            state->inflight.erase(key);
            if (!bytes.empty()) state->pending.push_back({key, std::move(bytes)});
            else state->failed.insert(key);
        }
        // Resource visibility and its generation are published together. A
        // callback racing frame capture always leaves its generation pending.
        state->source.Invalidate(xframes::FrameReason::Resource);
    }
    state->source.Notify();
}

#ifndef __EMSCRIPTEN__
void MapView::DownloadTile(std::weak_ptr<AsyncState> weak, TileKey key, bool prefetch,
                           std::string url, std::unordered_map<std::string, std::string> headers) {
    auto state = weak.lock();
    if (!state || !state->alive) return;
    std::vector<unsigned char> bytes;
    int tier = 0;
    try {
        auto& cache = TileCache::getGlobalInstance();
        if (auto cached = cache.get(url)) { bytes = std::move(*cached); tier = 1; }
        else if (state->disk->isEnabled()) {
            if (auto cached = state->disk->get(key.x, key.y, key.zoom)) {
                bytes = std::move(*cached); tier = 2;
            }
        }
        if (bytes.empty() && state->alive) {
            fetchTile(url, headers, [&](bool success, std::vector<uint8_t> data) {
                if (success) bytes.assign(data.begin(), data.end());
            });
        }
        if (!bytes.empty() && state->alive) {
            cache.put(url, bytes.data(), bytes.size());
            if (state->disk->isEnabled()) state->disk->put(key.x, key.y, key.zoom, bytes.data(), bytes.size());
        }
    } catch (const std::exception& error) {
        fprintf(stderr, "Map tile failed: %s\n", error.what());
        bytes.clear();
    }
    CompleteTile(weak, key, prefetch, std::move(bytes), tier);
}
#endif

bool MapView::StartDownload(TileKey key, bool prefetch) {
    const auto weak = std::weak_ptr<AsyncState>(m_async);
    const auto url = BuildTileUrl(key.x, key.y, key.zoom);
    {
        const std::lock_guard lock(m_async->mutex);
        if (prefetch) ++m_async->prefetchInflight;
        else m_async->inflight.insert(key);
    }
#ifdef __EMSCRIPTEN__
    // Browser cache lookups are synchronous, but still go through the same
    // mailbox. The next frame consumes their bytes before completing coverage.
    auto& cache = TileCache::getGlobalInstance();
    if (auto cached = cache.get(url)) {
        if (prefetch && m_async->disk->isEnabled())
            m_async->disk->put(key.x, key.y, key.zoom, cached->data(), cached->size());
        CompleteTile(weak, key, prefetch, std::move(*cached), 1);
    } else if (auto cached = m_async->disk->isEnabled() ? m_async->disk->get(key.x, key.y, key.zoom) : std::nullopt) {
        CompleteTile(weak, key, prefetch, std::move(*cached), 2);
    } else {
        const auto requestKey = std::string(prefetch ? "prefetch:" : "tile:") + url;
        m_fetches.Get(requestKey, url, [weak, key, prefetch, url](bool success, WasmFetches::Bytes bytes) {
            auto state = weak.lock();
            if (!state || !state->alive) return;
            try {
                if (success && !bytes.empty()) {
                    TileCache::getGlobalInstance().put(url, bytes.data(), bytes.size());
                    if (state->disk->isEnabled()) state->disk->put(key.x, key.y, key.zoom, bytes.data(), bytes.size());
                }
            } catch (const std::exception& error) {
                fprintf(stderr, "Map cache failed: %s\n", error.what());
                success = false;
            }
            CompleteTile(weak, key, prefetch, success ? std::move(bytes) : WasmFetches::Bytes{}, 0);
        }, m_tileRequestHeaders);
    }
    return true;
#else
    if (m_view->GetMapWorker().Submit(weak, [weak, key, prefetch, url, headers = m_tileRequestHeaders] {
        DownloadTile(weak, key, prefetch, url, headers);
    })) return true;
    const std::lock_guard lock(m_async->mutex);
    if (prefetch) --m_async->prefetchInflight;
    else m_async->inflight.erase(key);
    return false;
#endif
}

void MapView::PumpPrefetch() {
    // Four outstanding requests per Map; remaining requested tiles stay owned
    // until actual completions wake preparation or the Map is removed.
    for (int started = 0; started < 4 && !m_prefetchQueue.empty(); ++started) {
        {
            const std::lock_guard lock(m_async->mutex);
            if (m_async->prefetchInflight >= 4) break;
        }
        if (!StartDownload(m_prefetchQueue.front(), true)) break;
        m_prefetchQueue.pop_front();
    }
}

void MapView::FetchMissingTiles(int xMin, int xMax, int yMin, int yMax) {
    const int maxTiles = 1 << m_zoom;
    std::set<TileKey> requested;
    for (int x = xMin; x < xMax; ++x) {
        for (int y = std::max(0, yMin); y < std::min(maxTiles, yMax); ++y)
            requested.insert({((x % maxTiles) + maxTiles) % maxTiles, y, m_zoom});
    }
    if (requested != m_requestedKeys) {
        const std::lock_guard lock(m_async->mutex);
        // A finite viewport attempt does not retry failures on each completion.
        // A subsequent view request permits retry, without periodic activity.
        m_async->failed.clear();
        m_requestedKeys = requested;
    }
    for (const auto& key : requested) {
        if (m_tileTextures.contains(key)) continue;
        {
            const std::lock_guard lock(m_async->mutex);
            if (m_async->inflight.size() + m_async->pending.size() >= 64) break;
            if (m_async->inflight.contains(key) || m_async->failed.contains(key)) continue;
        }
        if (!StartDownload(key, false)) break;
    }
}

void MapView::PrepareFrame(XFrames* view) {
    m_resourceLifetime.Set(false);
    std::vector<PendingTile> pending;
    {
        const std::lock_guard lock(m_async->mutex);
        pending.swap(m_async->pending);
        m_cacheStats = m_async->stats;
        m_prefetchCompleted = m_async->prefetchCompleted;
        m_prefetchTotal = m_async->prefetchTotal;
        m_prefetching = m_prefetchCompleted < m_prefetchTotal;
        if (m_async->progressPending) {
            view->QueuePrefetchProgress(m_async->source, m_id, m_prefetchCompleted, m_prefetchTotal);
            m_async->progressPending = false;
        }
    }
    for (auto& tile : pending) {
        if (m_tileTextures.contains(tile.key)) continue;
        Texture texture;
        bool loaded = false;
        if (tile.pngData.size() <= INT_MAX) {
#ifdef __EMSCRIPTEN__
            loaded = view->m_renderer->LoadTexture(tile.pngData.data(), static_cast<int>(tile.pngData.size()), &texture);
#else
            texture.textureView = view->m_renderer->LoadTexture(tile.pngData.data(), static_cast<int>(tile.pngData.size()));
            loaded = texture.textureView != 0;
#endif
        }
        if (loaded) {
            while (m_tileTextures.size() >= MAX_GPU_TILES) {
                auto oldest = m_tileTextures.find(m_textureLruOrder.back());
                view->m_renderer->RetireTexture(oldest->second.first);
                m_tileTextures.erase(oldest);
                m_textureLruOrder.pop_back();
            }
            m_textureLruOrder.push_front(tile.key);
            m_tileTextures[tile.key] = {texture, m_textureLruOrder.begin()};
        } else {
            const std::lock_guard lock(m_async->mutex);
            m_async->failed.insert(tile.key);
            m_async->lastLoadFailed = true;
        }
    }
    PumpPrefetch();
}

json MapView::GetResourceDiagnostics() const {
    const std::lock_guard lock(m_async->mutex);
    return {{"loadedTextures", m_tileTextures.size()}, {"queuedLoads", m_async->pending.size()},
        {"pendingRequests", m_async->inflight.size() + m_async->prefetchInflight},
        {"queuedPrefetch", m_prefetchQueue.size()}, {"prefetchCompleted", m_async->prefetchCompleted},
        {"prefetchTotal", m_async->prefetchTotal}, {"failedTiles", m_async->failed.size()},
        {"lastLoadFailed", m_async->lastLoadFailed}, {"zoomDebouncing", m_zoomDebouncing}};
}

void MapView::Render(XFrames* view, const std::optional<ImRect>& viewport) {
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
            m_lastZoomChangeTime = view->m_frameScheduler.Now();
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
                m_lastZoomChangeTime = view->m_frameScheduler.Now();
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

    // Expand fetch range in pan direction for prefetching
    int fetchXMin = xMin + std::min(m_panDirX, 0);
    int fetchXMax = xMax + std::max(m_panDirX, 0);
    int fetchYMin = yMin + std::min(m_panDirY, 0);
    int fetchYMax = yMax + std::max(m_panDirY, 0);

    // Debounce tile fetches during rapid zoom (150ms after last scroll)
    if (m_zoomDebouncing) {
        auto elapsed = view->m_frameScheduler.Now() - m_lastZoomChangeTime;
        if (elapsed >= std::chrono::milliseconds(150)) {
            m_zoomDebouncing = false;
            FetchMissingTiles(fetchXMin, fetchXMax, fetchYMin, fetchYMax);
        } else {
            m_resourceLifetime.Set(false, m_lastZoomChangeTime + std::chrono::milliseconds(150));
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
    bool tilesFailed;
    { const std::lock_guard lock(m_async->mutex); tilesFailed = !m_async->failed.empty(); }
    if (!allCurrentTilesLoaded) {
        const char* loadingText = tilesFailed ? "Some tiles unavailable" : "Loading...";
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
    if (m_async->disk->isEnabled()) {
        std::string statsText = "GPU: " + std::to_string(m_tileTextures.size()) + "/" + std::to_string(MAX_GPU_TILES) +
                                " Mem: " + std::to_string(m_cacheStats.memoryHits) +
                                " Disk: " + std::to_string(m_cacheStats.diskHits) +
                                " Net: " + std::to_string(m_cacheStats.networkFetches);

        if (m_prefetching) {
            statsText += "  Prefetching " + std::to_string(m_prefetchCompleted) +
                         "/" + std::to_string(m_prefetchTotal);
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
        ResetRequests();
        for (auto& [key, entry] : m_tileTextures)
            if (view->m_renderer) view->m_renderer->RetireTexture(entry.first);
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
            m_async->disk->configure(m_cachePath);
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
            m_centerTileX = lonToX(lon, m_zoom);
            m_centerTileY = latToY(lat, m_zoom);

            if (!m_initialized) {
                TileCache::getGlobalInstance().configure(1024, 3600000);
            }
            m_initialized = true;

            m_zoomDebouncing = false;
            m_resourceLifetime.Set(false);
            { const std::lock_guard lock(m_async->mutex); m_async->failed.clear(); }

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

            if (!m_async->disk->isEnabled()) return;
            { const std::lock_guard lock(m_async->mutex);
              if (m_async->prefetchCompleted < m_async->prefetchTotal) return; }

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
                        if (m_async->disk->get(wrappedX, y, z).has_value()) continue;
                        if (tilesToFetch.size() >= 65536) throw std::length_error("Map prefetch supports at most 65536 uncached tiles per request");
                        tilesToFetch.push_back({wrappedX, y, z});
                    }
                }
            }

            {
                const std::lock_guard lock(m_async->mutex);
                m_async->prefetchCompleted = 0;
                m_async->prefetchTotal = static_cast<int>(tilesToFetch.size());
                m_async->progressPending = true;
            }
            m_prefetching = !tilesToFetch.empty();
            for (const auto& tile : tilesToFetch) m_prefetchQueue.push_back({tile.x, tile.y, tile.zoom});
            PumpPrefetch();
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
