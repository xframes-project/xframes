#include <GLES3/gl3.h>

#include "widget/separator.h"
#include "reactimgui.h"

void Separator::Render(ReactImgui* view, const std::optional<ImRect>& viewport) {
    ImGui::Separator();
};