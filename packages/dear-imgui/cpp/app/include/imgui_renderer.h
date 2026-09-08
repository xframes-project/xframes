#ifndef IMGUI_VIEW
#define IMGUI_VIEW

#include <fmt/core.h>

#include "IconsFontAwesome6.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"

#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <array>
#include <atomic>
#include "frame_scheduler.h"

#ifdef __EMSCRIPTEN__
#include "imgui_impl_wgpu.h"
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>
#endif

#include <texture_helpers.h>
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>


#include "./shared.h"

class XFrames;

using json = nlohmann::json;

class ImGuiRenderer {
    protected:
        GLFWwindow* m_glfwWindow = nullptr;

        std::string m_rawFontDefs;

        XFrames* m_xframes;

        const char* m_glWindowTitle;

        int m_initial_window_width = 900;
        int m_initial_window_height = 700;
        int m_window_width = m_initial_window_width;
        int m_window_height = m_initial_window_height;

        std::string m_assetsBasePath;

        std::unordered_map<std::string, std::unordered_map<int, int>, StringHash, std::equal_to<>> m_fontDefMap;

        // static constexpr ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };

        void LoadFontsFromDefs();
        void InstallWindowCallbacks();
        void ProcessWindowRequests();
        bool UpdateSurfaceAvailability();
        void DrawFrame();
        void UpdateImGuiActivity();
        void Invalidate(xframes::FrameReason reason);
        static void WakeRenderer(void* context) noexcept;
        std::mutex m_windowRequestMutex;
        std::optional<std::pair<int, int>> m_requestedSize;
        bool m_sizeSuppressed = false;
        bool m_loopRunning = false;
        std::atomic<unsigned> m_windowCallbackCount{0};
        bool m_resumingFromIdle = true;
        std::optional<xframes::FrameScheduler::Time> m_previousFrameTime;
        std::array<xframes::FrameScheduler::Owner, 5> m_imguiActivity;
        unsigned m_failedSubmissions = 0;
        std::mutex m_retiredTextureMutex;
        std::vector<Texture> m_retiredTextures;
        size_t m_liveResourceTextures = 0; // retired-texture mutex; excludes backend font textures

    #ifdef __EMSCRIPTEN__
        std::unique_ptr<char[]> m_canvasSelector;
        wgpu::Instance m_instance;
        WGPUColor m_clearColor;
        WGPUDevice m_device = nullptr;
        WGPUQueue m_queue = nullptr;
        WGPUSurface m_wgpu_surface = nullptr;
        WGPUTextureFormat m_wgpu_preferred_fmt = WGPUTextureFormat_RGBA8Unorm;
        WGPUSurfaceConfiguration m_wgpu_surface_config = {};
        int m_wgpu_surface_width = 0;
        int m_wgpu_surface_height = 0;
        long m_browserFrame = 0;
        long m_browserTimer = 0;
        bool m_browserHidden = false;
        bool m_browserKeepalive = false;
        bool m_visibilityListener = false;
        void RequestBrowserFrame();
        void ScheduleBrowserNext();
        void BrowserFrame();
        void StopBrowserScheduling();
    #else
        ImVec4 m_clearColor;

        struct ScreenshotRequest {
            std::string path;
            std::function<void(std::optional<std::string>)> callback;
            uint64_t generation = 0;
        };

        std::mutex m_screenshotMutex;
        std::queue<ScreenshotRequest> m_screenshotRequests;
        bool m_acceptScreenshotRequests = false;

        void StartScreenshotRequests();
        void StopScreenshotRequests(const std::string& errorMessage);
        void FailScreenshotRequests(const std::string& errorMessage);
    #endif

    public:
        virtual ~ImGuiRenderer() = default;
        ImGuiRenderer(
            XFrames* xframes,
            const char* newWindowId,
            const char* newGlWindowTitle,
            std::string rawFontDefs,
            const std::optional<std::string>& basePath
        );

        ImGuiContext* m_imGuiCtx;

        bool m_shouldLoadDefaultStyle;

        std::vector<ImFont*> m_loadedFonts;

        const char* m_windowId;

#ifdef __EMSCRIPTEN__
        bool LoadTexture(const void* data, int numBytes, Texture* texture);
#else
        bool LoadTextureFile(const std::string& url, Texture* texture);
        GLuint LoadTexture(const void* data, int numBytes);
        void RequestScreenshot(
            std::string path,
            std::function<void(std::optional<std::string>)> callback
        );
        void FlushScreenshotRequests();
        std::optional<std::string> CaptureScreenshotToPng(const std::string& path);
#endif
        // virtual void PrepareForRender() = 0;
        // virtual void Render(int window_width, int window_height) = 0;

        void BeginRenderLoop();

        int GetFontIndex(const std::string& fontName, int fontSize);

        [[nodiscard]] bool IsFontIndexValid(int fontIndex) const;

        void SetFontDefault(int fontIndex) const;

        // Only call this after having verified that fontIndex is valid (use IsFontIndexValid())
        void PushFont(int fontIndex) const;

        void PopFont();

        ImGuiStyle& GetStyle();

        virtual void SetCurrentContext();

        virtual void SetUp();

        void InitGlfw();

    #ifdef __EMSCRIPTEN__
        bool InitWGPU();
        void RenderDrawData(WGPURenderPassEncoder pass);
        void ConfigureSurface(int width, int height);
        virtual void Init(std::string& cs);
    #else
        void RenderDrawData();
        virtual void Init();
    #endif
        void HandleScreenSizeChanged();

        bool PerformRendering();
        // Called by XFrames after coherent frame capture, under its tree locks.
        // Unit-test renderers with no platform window only construct ImGui data.
        bool PrepareFrame(int& width, int& height);
        void FinishFrame();
        bool HasPlatformWindow() const { return m_glfwWindow != nullptr; }

        virtual void CleanUp();
        void StopScheduling();

        // Mutation/destruction relinquishes ownership here. Release runs before
        // the next frame, after any older draw data using this texture submits.
        void RetireTexture(Texture texture);
        void ReleaseRetiredTextures();
        json GetResourceDiagnostics();
        json GetPlatformDiagnostics();

        void SetWindowSize(int width, int height);

        json GetAvailableFonts();

        // Render-thread-only, queried by opt-in frame diagnostics.
        json GetDiagnosticsBackendInfo() const;
};

#endif
