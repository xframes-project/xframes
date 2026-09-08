#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/html5.h>
#include <emscripten/eventloop.h>
#include <webgpu/webgpu.h>
#else
#include "imgui_impl_opengl3.h"
#include <GLES3/gl3.h>
#endif

#include "./xframes.h"
#include "imgui_renderer.h"
#include "screenshot_writer.h"

#include <cstring>
#include <exception>
#include <utility>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

using xframes::FrameReason;
using xframes::FrameScheduler;

void glfw_error_callback(int error, const char* description)
{
    printf("GLFW Error %d: %s\n", error, description);
}


ImGuiRenderer::ImGuiRenderer(
    XFrames* xframes,
    const char* windowId,
    const char* glWindowTitle,
    std::string rawFontDefs,
    const std::optional<std::string>& basePath) {

    m_xframes = xframes;

    m_windowId = windowId;
    m_glWindowTitle = glWindowTitle;

    m_shouldLoadDefaultStyle = true;

    m_imGuiCtx = ImGui::CreateContext();

    m_rawFontDefs = std::move(rawFontDefs);

    m_clearColor = { 0.45f, 0.55f, 0.60f, 1.00f };

    m_assetsBasePath = basePath.value_or("assets");
}

void ImGuiRenderer::LoadFontsFromDefs() {
    ImGuiIO& io = m_imGuiCtx->IO;

    auto fontDefs = json::parse(m_rawFontDefs);

    static constexpr ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    // static const ImWchar icons_ranges[] = { ICON_MIN_MDI, ICON_MAX_16_MDI, 0 };

    if (fontDefs.is_object() && fontDefs.contains("defs") && fontDefs["defs"].is_array()) {
        for (auto& [key, item] : fontDefs["defs"].items()) {
            if (item.is_object()) {
                if (item.contains("name") && item.contains("size") && item["name"].is_string() && item["size"].is_number()) {
                    auto fontName = item["name"].template get<std::string>();
                    auto pathToFont = fmt::format("{}/fonts/{}.ttf", m_assetsBasePath, fontName);
                    auto fontSize = item["size"].template get<int>();

                    if (!m_fontDefMap.contains(fontName)) {
                        m_fontDefMap[fontName] = std::unordered_map<int, int>();
                    }

                    if (!m_fontDefMap[fontName].contains(fontSize)) {
                        m_fontDefMap[fontName][fontSize] = (int)m_loadedFonts.size();
                    }

                    m_loadedFonts.push_back(
                        io.Fonts->AddFontFromFileTTF(
                            pathToFont.c_str(),
                            fontSize
                        )
                    );

                    float iconFontSize = fontSize * 2.0f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly
                    ImFontConfig icons_config;
                    icons_config.MergeMode = true;
                    icons_config.PixelSnapH = true;
                    icons_config.GlyphMinAdvanceX = iconFontSize;
                    auto pathToFaFontFile = fmt::format("{}/fonts/{}", m_assetsBasePath, FONT_ICON_FILE_NAME_FAS);
                    // auto pathToMdiFontFile = std::format("assets/fonts/{}", FONT_ICON_FILE_NAME_MDI);

                    io.Fonts->AddFontFromFileTTF(pathToFaFontFile.c_str(), iconFontSize, &icons_config, icons_ranges);
                    // io.Fonts->AddFontFromFileTTF(pathToMdiFontFile.c_str(), fontSize, &icons_config, icons_ranges);
                }
            }
        }

        io.Fonts->Build();

        if (fontDefs.contains("defaultFont")
            && fontDefs["defaultFont"].is_object()
            && fontDefs["defaultFont"]["name"].is_string()
            && fontDefs["defaultFont"]["size"].is_number_unsigned()) {

            auto defaultFontName = fontDefs["defaultFont"]["name"].template get<std::string>();
            auto defaultFontSize = fontDefs["defaultFont"]["size"].template get<int>();

            if (m_fontDefMap.contains(defaultFontName) && m_fontDefMap[defaultFontName].contains(defaultFontSize)) {
                auto fontIndex = m_fontDefMap[defaultFontName][defaultFontSize];

                SetFontDefault(fontIndex);
            }
        } else {
            SetFontDefault(0);
        }
    }

    // If not custom fonts defined, ensure font-awesome are still available
    if (m_loadedFonts.size() == 0) {
        io.Fonts->AddFontDefault();
        float baseFontSize = 13.0f; // 13.0f is the size of the default font.
        float iconFontSize = baseFontSize * 2.0f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = iconFontSize;
        auto pathToFaFontFile = fmt::format("assets/fonts/{}", FONT_ICON_FILE_NAME_FAS);

        m_loadedFonts.push_back(
            io.Fonts->AddFontFromFileTTF(pathToFaFontFile.c_str(), iconFontSize, &icons_config, icons_ranges)
        );

        io.Fonts->Build();

        SetFontDefault(0);
    }
}

int ImGuiRenderer::GetFontIndex(const std::string& fontName, const int fontSize) {
    if (m_fontDefMap.contains(fontName) && m_fontDefMap[fontName].contains(fontSize)) {
        return m_fontDefMap[fontName][fontSize];
    }

    return -1;
}

bool ImGuiRenderer::IsFontIndexValid(const int fontIndex) const {
    return fontIndex >= 0 && fontIndex < m_loadedFonts.size();
}

void ImGuiRenderer::SetFontDefault(const int fontIndex) const {
    ImGuiIO& io = m_imGuiCtx->IO;

    if (IsFontIndexValid(fontIndex)) {
        io.FontDefault = m_loadedFonts[fontIndex];
    }
}

void ImGuiRenderer::PushFont(const int fontIndex) const {
    ImGui::PushFont(m_loadedFonts[fontIndex]);
}

void ImGuiRenderer::PopFont() {
    ImGui::PopFont();
}

ImGuiStyle& ImGuiRenderer::GetStyle() {
    return ImGui::GetStyle();
}

void ImGuiRenderer::InitGlfw() {
    glfwSetErrorCallback(glfw_error_callback);
    glfwInit();

#ifdef __EMSCRIPTEN__
    // Make sure GLFW does not initialize any graphics context.
    // This needs to be done explicitly later.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    m_glfwWindow = glfwCreateWindow(m_window_width, m_window_height, m_glWindowTitle, nullptr, nullptr);
    if (!m_glfwWindow) throw std::runtime_error("GLFW window creation failed");
    // ImGui installs its callbacks later, chaining these on this new window.
    InstallWindowCallbacks();

#ifdef __EMSCRIPTEN__
    // Initialize the WebGPU environment
    if (!InitWGPU())
    {
        if (m_glfwWindow)
            glfwDestroyWindow(m_glfwWindow);
        glfwTerminate();
        return;
    }
    glfwShowWindow(m_glfwWindow);
#else
    glfwMakeContextCurrent(m_glfwWindow);
    glfwSwapInterval(1); // Enable vsync

    ImGui_ImplGlfw_InitForOpenGL(m_glfwWindow, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
#endif
}

#ifdef __EMSCRIPTEN__
bool ImGuiRenderer::InitWGPU() {
    // Create instance with TimedWaitAny (required for synchronous adapter/device request)
    wgpu::InstanceDescriptor instance_desc = {};
    static constexpr wgpu::InstanceFeatureName timedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
    instance_desc.requiredFeatureCount = 1;
    instance_desc.requiredFeatures = &timedWaitAny;
    m_instance = wgpu::CreateInstance(&instance_desc);

    // Request adapter (synchronous via WaitAny)
    wgpu::Adapter acquired_adapter;
    wgpu::RequestAdapterOptions adapter_options;
    auto onAdapter = [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
        if (status == wgpu::RequestAdapterStatus::Success)
            acquired_adapter = std::move(adapter);
        else
            printf("Failed to get adapter: %s\n", message.data);
    };
    wgpu::Future adapterFuture { m_instance.RequestAdapter(&adapter_options, wgpu::CallbackMode::WaitAnyOnly, onAdapter) };
    m_instance.WaitAny(adapterFuture, UINT64_MAX);
    if (!acquired_adapter) return false;

    // Request device (synchronous via WaitAny)
    wgpu::DeviceDescriptor device_desc;
    // The one-shot lost callback owns this weak endpoint until the device ends.
    // WebGPU stops uncaptured-error callbacks when that device is lost.
    const auto source = std::make_shared<FrameScheduler::Source>(m_xframes->m_frameScheduler.GetSource());
    device_desc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
        [source](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView msg) {
            if (!source->IsAlive()) return; // Intentional shutdown or expired runtime.
            fprintf(stderr, "Device lost (%d): %s\n", (int)reason, msg.data);
            source->FailBackend();
        });
    device_desc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView msg, FrameScheduler::Source* source) {
            fprintf(stderr, "WebGPU error (%d): %s\n", (int)type, msg.data);
            source->FailBackend();
        }, source.get());
    wgpu::Device acquired_device;
    auto onDevice = [&](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
        if (status == wgpu::RequestDeviceStatus::Success)
            acquired_device = std::move(device);
        else
            printf("Failed to get device: %s\n", message.data);
    };
    wgpu::Future deviceFuture { acquired_adapter.RequestDevice(&device_desc, wgpu::CallbackMode::WaitAnyOnly, onDevice) };
    m_instance.WaitAny(deviceFuture, UINT64_MAX);
    if (!acquired_device) return false;
    m_device = acquired_device.MoveToCHandle();

    // Create surface from canvas
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc = {};
    canvas_desc.selector = m_canvasSelector.get();
    wgpu::SurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = &canvas_desc;
    wgpu::Surface surface = m_instance.CreateSurface(&surface_desc);
    m_wgpu_surface = surface.MoveToCHandle();
    if (!m_wgpu_surface) return false;

    // Get preferred format via surface capabilities
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(m_wgpu_surface, acquired_adapter.Get(), &caps);
    m_wgpu_preferred_fmt = caps.formats[0];

    // Cache queue
    m_queue = wgpuDeviceGetQueue(m_device);

    return true;
}
#endif

#ifdef __EMSCRIPTEN__
void ImGuiRenderer::SetUp() {
    InitGlfw();

    IMGUI_CHECKVERSION();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(m_glfwWindow, true);
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(m_glfwWindow, m_canvasSelector.get());

    ImGui_ImplWGPU_InitInfo init_info;
    init_info.Device = m_device;
    init_info.NumFramesInFlight = 3;
    init_info.RenderTargetFormat = m_wgpu_preferred_fmt;
    init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&init_info);

    // Initial surface configuration
    int width, height;
    glfwGetFramebufferSize(m_glfwWindow, &width, &height);
    ConfigureSurface(width, height);
}
#else
void ImGuiRenderer::SetUp() {
    InitGlfw();

    IMGUI_CHECKVERSION();

    // SetCurrentContext();
}
#endif

#ifdef __EMSCRIPTEN__
void ImGuiRenderer::ConfigureSurface(int width, int height) {
    if (width <= 0 || height <= 0) return;
    m_wgpu_surface_width = width;
    m_wgpu_surface_height = height;
    m_wgpu_surface_config.device = m_device;
    m_wgpu_surface_config.format = m_wgpu_preferred_fmt;
    m_wgpu_surface_config.usage = WGPUTextureUsage_RenderAttachment;
    m_wgpu_surface_config.presentMode = WGPUPresentMode_Fifo;
    m_wgpu_surface_config.alphaMode = WGPUCompositeAlphaMode_Auto;
    m_wgpu_surface_config.width = width;
    m_wgpu_surface_config.height = height;
    wgpuSurfaceConfigure(m_wgpu_surface, &m_wgpu_surface_config);
}
#endif

// todo: is this necessary for opengl rendering?
void ImGuiRenderer::HandleScreenSizeChanged() {
#ifdef __EMSCRIPTEN__
    int width, height;
    glfwGetFramebufferSize(m_glfwWindow, &width, &height);
    if (width != m_wgpu_surface_width || height != m_wgpu_surface_height)
    {
        ConfigureSurface(width, height);
    }
#endif
}

#ifdef __EMSCRIPTEN__
void ImGuiRenderer::RenderDrawData(WGPURenderPassEncoder pass) {
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
}
#else
void ImGuiRenderer::RenderDrawData() {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
#endif

void ImGuiRenderer::StopScheduling() {
    if (!m_loopRunning) return;
    m_xframes->m_frameScheduler.DetachWake();
    m_loopRunning = false;
    for (auto& activity : m_imguiActivity) activity.Reset();
#ifdef __EMSCRIPTEN__
    StopBrowserScheduling();
#endif
}

void ImGuiRenderer::CleanUp() {
    StopScheduling();
    m_xframes->Dispose();
    if (!m_imGuiCtx) return;
    ReleaseRetiredTextures();
    auto& io = m_imGuiCtx->IO;
#ifdef __EMSCRIPTEN__
    if (io.BackendRendererUserData) ImGui_ImplWGPU_Shutdown();
    if (m_wgpu_surface) { wgpuSurfaceUnconfigure(m_wgpu_surface); wgpuSurfaceRelease(m_wgpu_surface); m_wgpu_surface = nullptr; }
    if (m_queue) { wgpuQueueRelease(m_queue); m_queue = nullptr; }
    if (m_device) { wgpuDeviceRelease(m_device); m_device = nullptr; }
    if (m_instance) wgpuInstanceRelease(m_instance.MoveToCHandle());
#else
    if (io.BackendRendererUserData) ImGui_ImplOpenGL3_Shutdown();
#endif
    if (io.BackendPlatformUserData) ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(m_imGuiCtx);
    m_imGuiCtx = nullptr;
    m_loadedFonts.clear(); m_fontDefMap.clear();
    if (m_glfwWindow) glfwDestroyWindow(m_glfwWindow);
    m_glfwWindow = nullptr;
    m_windowCallbackCount = 0;
    glfwTerminate();
    glfwSetErrorCallback(nullptr);
#ifdef __EMSCRIPTEN__
    m_canvasSelector.reset();
    // Fetch cancellation can invoke a JS callback synchronously. Keep the
    // runtime alive until every callback/resource/backend owner is closed.
    if (m_browserKeepalive) { m_browserKeepalive = false; emscripten_runtime_keepalive_pop(); }
#endif
}

#ifdef __EMSCRIPTEN__
bool ImGuiRenderer::PerformRendering() {
    // Get current surface texture
    WGPUSurfaceTexture surface_texture{};
    wgpuSurfaceGetCurrentTexture(m_wgpu_surface, &surface_texture);
    if (ImGui_ImplWGPU_IsSurfaceStatusError(surface_texture.status)) {
        fprintf(stderr, "Unrecoverable surface texture status=%#.8x\n", surface_texture.status);
        if (surface_texture.texture) wgpuTextureRelease(surface_texture.texture);
        m_xframes->m_frameScheduler.GetSource().FailBackend();
        return false;
    }
    if (ImGui_ImplWGPU_IsSurfaceStatusSubOptimal(surface_texture.status)) {
        if (surface_texture.texture)
            wgpuTextureRelease(surface_texture.texture);
        return false;
    }
    if (!surface_texture.texture) return false;

    // Create view from surface texture
    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = m_wgpu_surface_config.format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
    view_desc.arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
    view_desc.aspect = WGPUTextureAspect_All;
    WGPUTextureView texture_view = wgpuTextureCreateView(surface_texture.texture, &view_desc);
    if (!texture_view) {
        wgpuTextureRelease(surface_texture.texture);
        return false;
    }

    WGPURenderPassColorAttachment color_attachments = {};
    color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color_attachments.loadOp = WGPULoadOp_Clear;
    color_attachments.storeOp = WGPUStoreOp_Store;
    color_attachments.clearValue = m_clearColor;
    color_attachments.view = texture_view;

    WGPURenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachments;
    render_pass_desc.depthStencilAttachment = nullptr;

    WGPUCommandEncoderDescriptor enc_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &enc_desc);
    if (!encoder) {
        wgpuTextureViewRelease(texture_view);
        wgpuTextureRelease(surface_texture.texture);
        return false;
    }
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
    if (!pass) {
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(texture_view);
        wgpuTextureRelease(surface_texture.texture);
        return false;
    }
    RenderDrawData(pass);
    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBufferDescriptor cmd_buffer_desc = {};
    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
    if (cmd_buffer) wgpuQueueSubmit(m_queue, 1, &cmd_buffer);

    // Release resources
    wgpuTextureViewRelease(texture_view);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(encoder);
    if (cmd_buffer) wgpuCommandBufferRelease(cmd_buffer);
    wgpuTextureRelease(surface_texture.texture);
    return cmd_buffer && m_xframes->m_frameScheduler.GetStatus() == FrameScheduler::Status::Running;
}
#else
bool ImGuiRenderer::PerformRendering() {
    int display_w, display_h;
    glfwGetFramebufferSize(m_glfwWindow, &display_w, &display_h);
    if (display_w <= 0 || display_h <= 0) return false;
    glViewport(0, 0, display_w, display_h);
    glClearColor(m_clearColor.x * m_clearColor.w, m_clearColor.y * m_clearColor.w, m_clearColor.z * m_clearColor.w, m_clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    RenderDrawData();
    const auto error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "OpenGL submission failed: %#x\n", error);
        m_xframes->m_frameScheduler.GetSource().FailBackend();
        return false;
    }
    return true;
}
#endif

void ImGuiRenderer::SetCurrentContext() {
    ImGui::SetCurrentContext(m_imGuiCtx);
}

void ImGuiRenderer::RetireTexture(Texture texture) {
    if (!texture.textureView) return;
    const std::lock_guard lock(m_retiredTextureMutex);
    m_retiredTextures.push_back(texture);
}

void ImGuiRenderer::ReleaseRetiredTextures() {
    std::vector<Texture> retired;
    {
        const std::lock_guard lock(m_retiredTextureMutex);
        retired.swap(m_retiredTextures);
        m_liveResourceTextures -= retired.size();
    }
    for (auto& texture : retired) {
#ifdef __EMSCRIPTEN__
        wgpuTextureViewRelease(texture.textureView);
#else
        glDeleteTextures(1, &texture.textureView);
#endif
    }
}

json ImGuiRenderer::GetResourceDiagnostics() {
    const std::lock_guard lock(m_retiredTextureMutex);
    return {{"liveTextures", m_liveResourceTextures}, {"retiredTextures", m_retiredTextures.size()}};
}

#ifndef __EMSCRIPTEN__
bool ImGuiRenderer::LoadTextureFile(const std::string& url, Texture* texture) {
    std::ifstream file(fmt::format("{}/{}", m_assetsBasePath, url), std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = file.tellg();
    if (size <= 0 || size > std::numeric_limits<int>::max()) return false;
    std::vector<unsigned char> data(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file) return false;
    texture->textureView = LoadTexture(data.data(), static_cast<int>(size));
    if (!texture->textureView) return false;
    stbi_info_from_memory(data.data(), static_cast<int>(size), &texture->width, &texture->height, nullptr);
    return true;
}

void ImGuiRenderer::RequestScreenshot(
    std::string path,
    std::function<void(std::optional<std::string>)> callback
) {
    std::optional<std::string> error;
    {
        const std::lock_guard<std::mutex> lock(m_screenshotMutex);
        if (!m_acceptScreenshotRequests || !m_xframes->m_frameScheduler.CanInvalidate()) {
            error = "Renderer is not ready for screenshot capture";
        } else if (m_screenshotRequests.size() >= 32) {
            error = "Screenshot request limit reached (32)";
        } else {
            m_screenshotRequests.push(ScreenshotRequest{std::move(path), std::move(callback)});
            m_screenshotRequests.back().generation = m_xframes->m_frameScheduler.Invalidate(FrameReason::Screenshot);
        }
    }

    if (error) {
        callback(std::move(error));
        return;
    }

    m_xframes->m_frameScheduler.Notify();
}

void ImGuiRenderer::StartScreenshotRequests() {
    const std::lock_guard<std::mutex> lock(m_screenshotMutex);
    m_acceptScreenshotRequests = true;
}

void ImGuiRenderer::StopScreenshotRequests(const std::string& errorMessage) {
    std::queue<ScreenshotRequest> requests;
    {
        const std::lock_guard<std::mutex> lock(m_screenshotMutex);
        m_acceptScreenshotRequests = false;
        std::swap(requests, m_screenshotRequests);
    }

    while (!requests.empty()) {
        auto request = std::move(requests.front());
        requests.pop();
        request.callback(errorMessage);
    }
}

void ImGuiRenderer::FailScreenshotRequests(const std::string& errorMessage) {
    std::queue<ScreenshotRequest> requests;
    {
        const std::lock_guard lock(m_screenshotMutex);
        requests.swap(m_screenshotRequests);
    }
    while (!requests.empty()) {
        auto request = std::move(requests.front());
        requests.pop();
        request.callback(errorMessage);
    }
}

void ImGuiRenderer::FlushScreenshotRequests() {
    std::queue<ScreenshotRequest> requests;
    {
        const std::lock_guard<std::mutex> lock(m_screenshotMutex);
        const auto covered = m_xframes->m_frameScheduler.CoveredGeneration();
        // Requests racing draw construction need a subsequent submission.
        while (!m_screenshotRequests.empty() && m_screenshotRequests.front().generation <= covered) {
            requests.push(std::move(m_screenshotRequests.front()));
            m_screenshotRequests.pop();
        }
    }

    while (!requests.empty()) {
        auto request = std::move(requests.front());
        requests.pop();

        std::optional<std::string> maybeError;
        try {
            maybeError = CaptureScreenshotToPng(request.path);
        } catch (const std::exception& error) {
            maybeError = fmt::format("Screenshot capture failed: {}", error.what());
        } catch (...) {
            maybeError = "Screenshot capture failed with an unknown error";
        }

        request.callback(std::move(maybeError));
    }
}

std::optional<std::string> ImGuiRenderer::CaptureScreenshotToPng(const std::string& path) {
    if (m_glfwWindow == nullptr) {
        return "Renderer window is not initialized";
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_glfwWindow, &width, &height);

    if (width <= 0 || height <= 0) {
        return fmt::format("Invalid framebuffer size: {}x{}", width, height);
    }

    const int channels = 4;
    const int strideBytes = width * channels;
    std::vector<unsigned char> pixels(static_cast<size_t>(strideBytes) * static_cast<size_t>(height));
    std::vector<unsigned char> flipped(pixels.size());

    while (glGetError() != GL_NO_ERROR) {}

    GLint previousPackAlignment = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);

    const GLenum glError = glGetError();
    if (glError != GL_NO_ERROR) {
        return fmt::format("glReadPixels failed with GL error 0x{:x}", static_cast<unsigned int>(glError));
    }

    for (int y = 0; y < height; y++) {
        const auto srcOffset = static_cast<size_t>(height - 1 - y) * static_cast<size_t>(strideBytes);
        const auto dstOffset = static_cast<size_t>(y) * static_cast<size_t>(strideBytes);
        memcpy(flipped.data() + dstOffset, pixels.data() + srcOffset, static_cast<size_t>(strideBytes));
    }

    return WritePngToFile(path, width, height, channels, flipped.data(), strideBytes);
}
#endif

void ImGuiRenderer::BeginRenderLoop() {
    SetUp();
    LoadFontsFromDefs();
    m_loopRunning = true;
    const FrameReason reasons[] = {FrameReason::Layout, FrameReason::Interaction,
        FrameReason::Cursor, FrameReason::KeyRepeat, FrameReason::Hover};
    for (size_t i = 0; i < m_imguiActivity.size(); ++i)
        m_imguiActivity[i] = m_xframes->m_frameScheduler.Register(reasons[i]);
#ifndef __EMSCRIPTEN__
    StartScreenshotRequests();
#else
    // A paused RAF loop still owns a live native runtime. Returning from init
    // must not run Emscripten's exit path while waiting for the next producer.
    emscripten_runtime_keepalive_push();
    m_browserKeepalive = true;
#endif
    m_xframes->m_frameScheduler.AttachWake(WakeRenderer, this);
    m_xframes->Init(this);
#ifdef __EMSCRIPTEN__
    EmscriptenVisibilityChangeEvent visibility{};
    emscripten_get_visibility_status(&visibility);
    m_browserHidden = visibility.hidden;
    emscripten_set_visibilitychange_callback(this, false,
        [](int, const EmscriptenVisibilityChangeEvent* event, void* data) -> bool {
            auto* self = static_cast<ImGuiRenderer*>(data);
            self->m_browserHidden = event->hidden;
            self->UpdateSurfaceAvailability();
            if (event->hidden) {
                if (self->m_browserFrame) emscripten_cancel_animation_frame(std::exchange(self->m_browserFrame, 0));
                if (self->m_browserTimer) emscripten_clear_timeout(std::exchange(self->m_browserTimer, 0));
                self->m_resumingFromIdle = true;
                self->m_xframes->m_frameScheduler.PlanNext();
            } else {
                self->Invalidate(FrameReason::Window);
                // Hidden tabs cancel the queued wake callback. Restoration must
                // restart it even when its scheduler notification was latched.
                self->RequestBrowserFrame();
            }
            return false;
        });
    m_visibilityListener = true;
    ScheduleBrowserNext();
#else
    while (!glfwWindowShouldClose(m_glfwWindow))
    {
        glfwPollEvents();
        ProcessWindowRequests();
        const bool available = UpdateSurfaceAvailability();
        if (glfwWindowShouldClose(m_glfwWindow)) break;
        const auto next = m_xframes->m_frameScheduler.TakeOpportunity();
        if (next.render) {
            DrawFrame();
        } else {
            if (next.terminal) StopScreenshotRequests("Renderer cannot submit: terminal native state");
            else if (!available) FailScreenshotRequests("Framebuffer is unavailable");
            if (!next.deadline) m_resumingFromIdle = true;
            // No event drain between the atomic wait arm and platform wait.
            // A racing producer's posted event remains queued for this wait.
            if (next.deadline) {
                const double seconds = std::chrono::duration<double>(*next.deadline - m_xframes->m_frameScheduler.Now()).count();
                if (seconds > 0) glfwWaitEventsTimeout(std::max(seconds, 0.001));
            } else glfwWaitEvents();
        }
    }
    StopScreenshotRequests("Renderer stopped before the screenshot could be captured");
    CleanUp();
#endif
}

json ImGuiRenderer::GetDiagnosticsBackendInfo() const {
    if (!m_glfwWindow) return {{"backend", "headless-unit-test"}};
#ifdef __EMSCRIPTEN__
    return {{"backend", "WebGPU"}};
#else
    const auto value = [](GLenum name) {
        const auto* text = glGetString(name);
        return text ? reinterpret_cast<const char*>(text) : "unavailable";
    };
    return {{"backend", "OpenGL"}, {"vendor", value(GL_VENDOR)},
        {"renderer", value(GL_RENDERER)}, {"version", value(GL_VERSION)}};
#endif
}

void ImGuiRenderer::SetWindowSize(int width, int height) {
    if (width < 0 || height < 0) throw std::invalid_argument("Window dimensions must be nonnegative");
    {
        const std::lock_guard lock(m_windowRequestMutex);
        m_requestedSize = std::pair(width, height);
        m_xframes->m_frameScheduler.Invalidate(FrameReason::Window);
    }
    m_xframes->m_frameScheduler.Notify();
}


#ifdef __EMSCRIPTEN__
void ImGuiRenderer::Init(std::string& cs) {
    m_canvasSelector = std::make_unique<char[]>(cs.length() + 1);
    strcpy(m_canvasSelector.get(), cs.c_str());

    BeginRenderLoop();
}
#else
void ImGuiRenderer::Init() {
    BeginRenderLoop();
}
#endif

#ifdef __EMSCRIPTEN__
bool ImGuiRenderer::LoadTexture(const void* data, const int numBytes, Texture* texture) {
    if (data == nullptr)
        return false;

    int width = 0;
    int height = 0;

    // TODO: figure out why we need the STB library to load image data for us, seems like I'm missing a step when using leptonica
    const auto stbiData = stbi_load_from_memory(static_cast<const stbi_uc*>(data), numBytes, &width, &height, nullptr, 4);
    if (!stbiData) return false;

    WGPUTextureView view;
    {
        WGPUTextureDescriptor tex_desc = {};
        tex_desc.label = { "texture", WGPU_STRLEN };
        tex_desc.dimension = WGPUTextureDimension_2D;
        tex_desc.size.width = width;
        tex_desc.size.height = height;
        tex_desc.size.depthOrArrayLayers = 1;
        tex_desc.sampleCount = 1;
        tex_desc.format = WGPUTextureFormat_RGBA8Unorm;
        tex_desc.mipLevelCount = 1;
        tex_desc.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;

        auto tex = wgpuDeviceCreateTexture(m_device, &tex_desc);
        if (!tex) { stbi_image_free(stbiData); return false; }

        WGPUTextureViewDescriptor tex_view_desc = {};
        tex_view_desc.format = WGPUTextureFormat_RGBA8Unorm;
        tex_view_desc.dimension = WGPUTextureViewDimension_2D;
        tex_view_desc.baseMipLevel = 0;
        tex_view_desc.mipLevelCount = 1;
        tex_view_desc.baseArrayLayer = 0;
        tex_view_desc.arrayLayerCount = 1;
        tex_view_desc.aspect = WGPUTextureAspect_All;
        view = wgpuTextureCreateView(tex, &tex_view_desc);
        if (!view) { wgpuTextureRelease(tex); stbi_image_free(stbiData); return false; }

        WGPUTexelCopyTextureInfo dst_view = {};
        dst_view.texture = tex;
        dst_view.mipLevel = 0;
        dst_view.origin = { 0, 0, 0 };
        dst_view.aspect = WGPUTextureAspect_All;
        WGPUTexelCopyBufferLayout layout = {};
        layout.offset = 0;
        layout.bytesPerRow = width * 4;
        layout.rowsPerImage = height;
        const WGPUExtent3D size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };

        wgpuQueueWriteTexture(m_queue, &dst_view, stbiData, static_cast<uint32_t>(width * 4 * height), &layout, &size);
        wgpuTextureRelease(tex); // The view retains the texture for drawing.
    }

    texture->textureView = view;
    texture->width = width;
    texture->height = height;

    stbi_image_free(stbiData);

    { const std::lock_guard lock(m_retiredTextureMutex); ++m_liveResourceTextures; }
    return true;
}
#else
GLuint ImGuiRenderer::LoadTexture(const void* data, int numBytes) {
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)numBytes, &image_width, &image_height, NULL, 4);
    if (image_data == NULL) {
        printf("Unable to load image from memory\n");
        return 0;
    }

    GLuint image_texture = 0;

    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("OpenGL Error: %d\n", error);
        glDeleteTextures(1, &image_texture);
        stbi_image_free(image_data);
        return 0;
    }

    stbi_image_free(image_data);

    { const std::lock_guard lock(m_retiredTextureMutex); ++m_liveResourceTextures; }
    return image_texture;
}
#endif

json ImGuiRenderer::GetAvailableFonts() {
    if (!m_imGuiCtx) return json::array();
    ImGuiIO& io = m_imGuiCtx->IO;
    json fonts = json::array();

    for (ImFont* font : io.Fonts->Fonts) {
        fonts.push_back(font->GetDebugName());
    }

    return fonts;
};
