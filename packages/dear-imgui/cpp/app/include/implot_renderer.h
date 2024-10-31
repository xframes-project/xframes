#ifndef IMPLOT_VIEW
#define IMPLOT_VIEW

#include "implot.h"

#include "imgui_renderer.h"

class XFrames;

class ImPlotRenderer : public ImGuiRenderer {
    protected:
        ImPlotContext* m_imPlotCtx;

    public:
        ImPlotRenderer(
            XFrames* xframes,
            const char* newWindowId,
            const char* newGlWindowTitle,
            std::string& rawFontDefs,
            std::optional<std::string> basePath
        );

        void SetCurrentContext();

        void CleanUp();
};

#endif