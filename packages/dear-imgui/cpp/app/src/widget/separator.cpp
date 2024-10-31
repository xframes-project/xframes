#include "widget/separator.h"
#include "xframes.h"

void Separator::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::Separator();
};