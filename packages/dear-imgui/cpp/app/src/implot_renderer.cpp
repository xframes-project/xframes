#include "implot.h"
#include "implot_internal.h"

#include "implot_renderer.h"

#include <utility>

ImPlotRenderer::ImPlotRenderer(
    XFrames* xframes,
    const char* newWindowId,
    const char* newGlWindowTitle,
    std::string& rawFontDefs,
    std::optional<std::string> basePath) : ImGuiRenderer(xframes, newWindowId, newGlWindowTitle, rawFontDefs, std::move(basePath)) {

    m_imPlotCtx = ImPlot::CreateContext();
}

void ImPlotRenderer::SetCurrentContext() {
    ImGuiRenderer::SetCurrentContext();

    ImPlot::SetCurrentContext(m_imPlotCtx);
};

void ImPlotRenderer::CleanUp() {
    ImGuiRenderer::CleanUp();

    ImPlot::DestroyContext(m_imPlotCtx);
};