#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#include <webgpu/webgpu.h>
#else
#include "imgui_impl_opengl3.h"
#include <GLES3/gl3.h>
#endif

#ifdef __EMSCRIPTEN__
#include <functional>
static std::function<void()>            MainLoopForEmscriptenP;
static void MainLoopForEmscripten()     { MainLoopForEmscriptenP(); }
#define EMSCRIPTEN_MAINLOOP_BEGIN       MainLoopForEmscriptenP = [&]()
#define EMSCRIPTEN_MAINLOOP_END         ; emscripten_set_main_loop(MainLoopForEmscripten, 30, true) // 24 frames / second, use 0 for browser's default
#else
#define EMSCRIPTEN_MAINLOOP_BEGIN
#define EMSCRIPTEN_MAINLOOP_END
#endif

#include "./xframes.h"
#include "imgui_renderer.h"

#include <utility>

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
    device_desc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
        [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView msg) {
            fprintf(stderr, "Device lost (%d): %s\n", (int)reason, msg.data);
        });
    device_desc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView msg) {
            fprintf(stderr, "WebGPU error (%d): %s\n", (int)type, msg.data);
        });
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

void ImGuiRenderer::CleanUp() {
#ifdef __EMSCRIPTEN__
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(m_imGuiCtx);

    wgpuSurfaceUnconfigure(m_wgpu_surface);
    wgpuSurfaceRelease(m_wgpu_surface);
    wgpuQueueRelease(m_queue);
    wgpuDeviceRelease(m_device);
    wgpuInstanceRelease(m_instance.MoveToCHandle());
#else
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(m_imGuiCtx);

    glfwDestroyWindow(m_glfwWindow);
    glfwTerminate();
    glfwSetErrorCallback(nullptr);
#endif
}

#ifdef __EMSCRIPTEN__
void ImGuiRenderer::PerformRendering() {
    // Get current surface texture
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(m_wgpu_surface, &surface_texture);
    if (ImGui_ImplWGPU_IsSurfaceStatusError(surface_texture.status)) {
        fprintf(stderr, "Unrecoverable surface texture status=%#.8x\n", surface_texture.status);
        return;
    }
    if (ImGui_ImplWGPU_IsSurfaceStatusSubOptimal(surface_texture.status)) {
        if (surface_texture.texture)
            wgpuTextureRelease(surface_texture.texture);
        return;
    }

    // Create view from surface texture
    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = m_wgpu_surface_config.format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
    view_desc.arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
    view_desc.aspect = WGPUTextureAspect_All;
    WGPUTextureView texture_view = wgpuTextureCreateView(surface_texture.texture, &view_desc);

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
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
    RenderDrawData(pass);
    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBufferDescriptor cmd_buffer_desc = {};
    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
    wgpuQueueSubmit(m_queue, 1, &cmd_buffer);

    // Release resources
    wgpuTextureViewRelease(texture_view);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd_buffer);
}
#else
void ImGuiRenderer::PerformRendering() {
    int display_w, display_h;
    glfwGetFramebufferSize(m_glfwWindow, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(m_clearColor.x * m_clearColor.w, m_clearColor.y * m_clearColor.w, m_clearColor.z * m_clearColor.w, m_clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    RenderDrawData();
}
#endif

void ImGuiRenderer::SetCurrentContext() {
    ImGui::SetCurrentContext(m_imGuiCtx);
}

#ifndef __EMSCRIPTEN__
void ImGuiRenderer::HandleNextImageJob() {
    auto& [widgetId, url] = m_xframes->m_imageJobs.front();

    auto pathToFile = fmt::format("{}/{}", m_assetsBasePath, url);

    FILE* f = fopen(pathToFile.c_str(), "rb");
    if (f == NULL) {
        printf("Unable to open file\n");
        return;
    }

    fseek(f, 0, SEEK_END);

    size_t file_size = (size_t)ftell(f);

    if (file_size == -1) {
        printf("Unable to determine file size of image\n");
    }

    fseek(f, 0, SEEK_SET);

    void* file_data = IM_ALLOC(file_size);

    fread(file_data, 1, file_size, f);

    fclose(f);

    m_xframes->m_imageToTextureMap[widgetId] = LoadTexture(file_data, file_size);

    IM_FREE(file_data);

    m_xframes->m_imageJobs.pop();
};
#endif

void ImGuiRenderer::BeginRenderLoop() {
    SetUp();

#ifdef __EMSCRIPTEN__
    LoadFontsFromDefs();
#else
    LoadFontsFromDefs();
    // printf("Adding default font\n");
    // m_imGuiCtx->IO.Fonts->AddFontDefault();

    // m_imGuiCtx->IO.FontDefault = m_imGuiCtx->IO.Fonts->Fonts[0];

    // printf("Default font added\n");
#endif

    m_xframes->Init(this);

    // Main loop
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(m_glfwWindow))
#endif
    {
#ifndef __EMSCRIPTEN__
        glfwWaitEventsTimeout(1.0 / 30.0);
#else
        glfwPollEvents();
#endif
        glfwGetWindowSize(m_glfwWindow, &m_window_width, &m_window_height);

        HandleScreenSizeChanged();

#ifdef __EMSCRIPTEN__
        ImGui_ImplWGPU_NewFrame();
#else
        ImGui_ImplOpenGL3_NewFrame();
#endif

        ImGui_ImplGlfw_NewFrame();

#ifndef __EMSCRIPTEN__
        if (!m_xframes->m_imageJobs.empty()) {
            HandleNextImageJob();
        }
#endif

        m_xframes->Render(m_window_width, m_window_height);

        PerformRendering();

#ifndef __EMSCRIPTEN__
        glfwSwapBuffers(m_glfwWindow);
#endif
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    CleanUp();
}

void ImGuiRenderer::SetWindowSize(int width, int height) {
    m_window_width = width;
    m_window_height = height;

    if (m_glfwWindow) {
        glfwSetWindowSize(m_glfwWindow, width, height);
    }
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

    int width;
    int height;

    // TODO: figure out why we need the STB library to load image data for us, seems like I'm missing a step when using leptonica
    const auto stbiData = stbi_load_from_memory(static_cast<const stbi_uc*>(data), numBytes, &width, &height, nullptr, 4);

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

        WGPUTextureViewDescriptor tex_view_desc = {};
        tex_view_desc.format = WGPUTextureFormat_RGBA8Unorm;
        tex_view_desc.dimension = WGPUTextureViewDimension_2D;
        tex_view_desc.baseMipLevel = 0;
        tex_view_desc.mipLevelCount = 1;
        tex_view_desc.baseArrayLayer = 0;
        tex_view_desc.arrayLayerCount = 1;
        tex_view_desc.aspect = WGPUTextureAspect_All;
        view = wgpuTextureCreateView(tex, &tex_view_desc);

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
    }

    texture->textureView = view;
    texture->width = width;
    texture->height = height;

    stbi_image_free(stbiData);

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
        return 0;
    }

    stbi_image_free(image_data);

    return image_texture;
}
#endif

json ImGuiRenderer::GetAvailableFonts() {
    ImGuiIO& io = m_imGuiCtx->IO;
    json fonts = json::array();

    for (ImFont* font : io.Fonts->Fonts) {
        fonts.push_back(font->GetDebugName());
    }

    return fonts;
};