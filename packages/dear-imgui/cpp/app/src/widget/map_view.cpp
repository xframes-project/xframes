#include <imgui.h>
#include <thread>
#include <yoga/YGNodeLayout.h>

#include "mapgenerator.h"
#include "widget/map_view.h"
#include "xframes.h"
#include "imgui_renderer.h"

bool MapView::HasCustomWidth() {
    return false;
}

bool MapView::HasCustomHeight() {
    return false;
}

void MapView::Render(XFrames* view, const std::optional<ImRect>& viewport) {
#ifndef __EMSCRIPTEN__
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        if (m_pendingTexture) {
            GLuint textureId = m_view->m_renderer->LoadTexture(
                m_pendingTexture->data.data(),
                static_cast<int>(m_pendingTexture->data.size())
            );
            if (textureId != 0) {
                auto tex = std::make_unique<Texture>();
                tex->textureView = textureId;
                tex->width = m_pendingTexture->width;
                tex->height = m_pendingTexture->height;
                m_textures[0] = std::move(tex);
                m_offset = m_pendingTexture->offset;
            }
            m_pendingTexture.reset();
        }
    }
#endif

    if (m_textures.contains(0)) {

        auto imageSize = ImVec2(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));

        if (imageSize.x != 0 && imageSize.y != 0) {
            ImGui::PushID(m_id);
            // ImGui::Text("offset: %f, %f", m_offset.x, m_offset.y);

            ImGui::BeginGroup();

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            double boxWidthPct = imageSize.x / m_textures[0]->width;
            double boxHeightPct = imageSize.y / m_textures[0]->height;

            ImGui::InvisibleButton("##map_canvas", imageSize);

            bool isDragging = ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);

            if (isDragging) {
                m_offset.x += ImGui::GetIO().MouseDelta.x;
                m_offset.y += ImGui::GetIO().MouseDelta.y;

                if (m_offset.x < 0) {
                    m_offset.x = 0;
                }

                if (m_offset.y < 0) {
                    m_offset.y = 0;
                }

                if (m_offset.x > (m_textures[0]->width - imageSize.x)) {
                    m_offset.x = m_textures[0]->width - imageSize.x;
                }

                if (m_offset.y > (m_textures[0]->height - imageSize.y)) {
                    m_offset.y = m_textures[0]->height - imageSize.y;
                }

                m_wasDragging = true;
            }

            if (m_wasDragging && !isDragging) {
                m_wasDragging = false;

                // Convert pixel offset from texture center to new lat/lon
                float viewCenterX = m_offset.x + imageSize.x / 2.0f;
                float viewCenterY = m_offset.y + imageSize.y / 2.0f;
                float texCenterX = m_textures[0]->width / 2.0f;
                float texCenterY = m_textures[0]->height / 2.0f;

                float deltaPixelsX = viewCenterX - texCenterX;
                float deltaPixelsY = viewCenterY - texCenterY;

                int tileSize = 256;
                double centerTileX = lonToX(m_centerLon, m_zoom);
                double centerTileY = latToY(m_centerLat, m_zoom);

                double newTileX = centerTileX + static_cast<double>(deltaPixelsX) / tileSize;
                double newTileY = centerTileY + static_cast<double>(deltaPixelsY) / tileSize;

                double newLon = xToLon(newTileX, m_zoom);
                double newLat = yToLat(newTileY, m_zoom);

                json reRenderOp = {{"op", "render"}, {"centerX", newLon}, {"centerY", newLat}, {"zoom", m_zoom}};
                HandleInternalOp(reRenderOp);
            }

            if (!ImGui::IsItemVisible()) {
                // Skip rendering as ImDrawList elements are not clipped.
                ImGui::EndGroup();
                ImGui::PopID();
                return;
            }

            const ImVec2 p0 = ImGui::GetItemRectMin();
            const ImVec2 p1 = ImGui::GetItemRectMax();

            ImGui::PushClipRect(p0, p1, true);

            ImVec2 uvMin = ImVec2(m_offset.x / m_textures[0]->width, m_offset.y / m_textures[0]->height);
            ImVec2 uvMax = ImVec2((m_offset.x / m_textures[0]->width) + boxWidthPct, (m_offset.y / m_textures[0]->height) + boxHeightPct);

            drawList->AddImage((void*)m_textures[0]->textureView, p0, p1, uvMin, uvMax);
            ImGui::PopClipRect();

            ImGui::EndGroup();
            ImGui::PopID();
        }
    }
};

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

            auto centerX = opDef["centerX"].template get<double>();
            auto centerY = opDef["centerY"].template get<double>();
            auto zoom = opDef["zoom"].template get<int>();

            m_centerLon = centerX;
            m_centerLat = centerY;
            m_zoom = zoom;

            int mapWidth = static_cast<int>(YGNodeLayoutGetWidth(m_layoutNode->m_node));
            int mapHeight = static_cast<int>(YGNodeLayoutGetHeight(m_layoutNode->m_node));

            if (mapWidth <= 0) mapWidth = 600;
            if (mapHeight <= 0) mapHeight = 600;

            int texWidth = mapWidth * BUFFER_MULTIPLIER;
            int texHeight = mapHeight * BUFFER_MULTIPLIER;

            MapGeneratorOptions options;
            options.m_width = texWidth;
            options.m_height = texHeight;
            options.m_tileRequestHeaders["User-Agent"] = "xframes/1.0";

            // Compute centered offset — applied when texture arrives (no flicker)
            ImVec2 centeredOffset(
                static_cast<float>((texWidth - mapWidth) / 2),
                static_cast<float>((texHeight - mapHeight) / 2)
            );

            m_mapGeneratorJobCounter++;

            m_mapGeneratorJobs[m_mapGeneratorJobCounter] = std::make_unique<MapGenerator>(options, [this, texWidth, texHeight, centeredOffset] (void* data, const size_t numBytes) {
#ifdef __EMSCRIPTEN__
                Texture texture{};
                const bool ret = m_view->m_renderer->LoadTexture(data, static_cast<int>(numBytes), &texture);
                IM_ASSERT(ret);
                m_textures[0] = std::make_unique<Texture>(texture);
#else
                {
                    std::lock_guard<std::mutex> lock(m_pendingMutex);
                    m_pendingTexture = PendingTexture{
                        std::vector<unsigned char>(
                            static_cast<unsigned char*>(data),
                            static_cast<unsigned char*>(data) + numBytes
                        ),
                        texWidth,
                        texHeight,
                        centeredOffset
                    };
                }
#endif
            });

            // Run tile download on background thread to avoid blocking the render loop
            auto job = m_mapGeneratorJobs[m_mapGeneratorJobCounter].get();
            std::thread([job, centerX, centerY, zoom]() {
                job->Render(std::make_tuple(centerX, centerY), zoom);
            }).detach();
        }
    }
};