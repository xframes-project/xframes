#include <GLES3/gl3.h>

#include <imgui.h>
#include <yoga/YGNodeLayout.h>

#include "mapgenerator.h"
#include "widget/map_view.h"
#include "reactimgui.h"
#include "imgui_renderer.h"

bool MapView::HasCustomWidth() {
    return false;
}

bool MapView::HasCustomHeight() {
    return false;
}

void MapView::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
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
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
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

// void ReactImgui::RenderMap(int id, double centerX, double centerY, int zoom)
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

            MapGeneratorOptions options;
            options.m_width = 600;
            options.m_height = 600;

            m_mapGeneratorJobCounter++;

            m_mapGeneratorJobs[m_mapGeneratorJobCounter] = std::make_unique<MapGenerator>(options, [this] (const void* data, const size_t numBytes) {
                Texture texture{};

                const bool ret = m_view->m_renderer->LoadTexture(data, static_cast<int>(numBytes), &texture);
                IM_ASSERT(ret);

                // TODO: add proper texture management
                m_textures[0] = std::make_unique<Texture>(texture);

                printf("Loaded texture, width: %d, height: %d\n", m_textures[0]->width, m_textures[0]->height);

                // TODO: remove job from map
            });

            m_mapGeneratorJobs[m_mapGeneratorJobCounter]->Render(std::make_tuple(centerX, centerY), zoom);
        }
    }
};