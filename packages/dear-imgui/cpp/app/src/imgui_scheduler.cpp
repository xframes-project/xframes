#include "imgui_renderer.h"
#include "xframes.h"
#include <algorithm>
#include <cmath>
#include <utility>
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#include <emscripten/eventloop.h>
#else
#include "imgui_impl_opengl3.h"
#endif

using xframes::FrameReason;
using xframes::FrameScheduler;

void ImGuiRenderer::Invalidate(FrameReason reason) {
    m_xframes->m_frameScheduler.Invalidate(reason);
    m_xframes->m_frameScheduler.Notify();
}
void ImGuiRenderer::WakeRenderer(void* context) noexcept {
#ifdef __EMSCRIPTEN__
    // Same browser thread. This only requests a future callback; it cannot
    // render or invoke application handlers while the scheduler mutex is held.
    static_cast<ImGuiRenderer*>(context)->RequestBrowserFrame();
#else
    glfwPostEmptyEvent();
#endif
}
void ImGuiRenderer::InstallWindowCallbacks() {
    glfwSetWindowUserPointer(m_glfwWindow, this);
    glfwSetCursorPosCallback(m_glfwWindow, [](GLFWwindow* w, double, double) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Input);
    });
    glfwSetCursorEnterCallback(m_glfwWindow, [](GLFWwindow* w, int) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Input);
    });
    glfwSetMouseButtonCallback(m_glfwWindow, [](GLFWwindow* w, int, int, int) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Input);
    });
    glfwSetScrollCallback(m_glfwWindow, [](GLFWwindow* w, double, double) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Input);
    });
    glfwSetKeyCallback(m_glfwWindow, [](GLFWwindow* w, int, int, int, int) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Input);
    });
    glfwSetCharCallback(m_glfwWindow, [](GLFWwindow* w, unsigned int) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Input);
    });
    glfwSetWindowFocusCallback(m_glfwWindow, [](GLFWwindow* w, int) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Input);
    });
    glfwSetWindowRefreshCallback(m_glfwWindow, [](GLFWwindow* w) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Window);
    });
    glfwSetWindowSizeCallback(m_glfwWindow, [](GLFWwindow* w, int, int) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Window);
    });
    glfwSetFramebufferSizeCallback(m_glfwWindow, [](GLFWwindow* w, int, int) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Window);
    });
#ifndef __EMSCRIPTEN__
    glfwSetWindowContentScaleCallback(m_glfwWindow, [](GLFWwindow* w, float, float) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Window);
    });
#endif
    glfwSetWindowIconifyCallback(m_glfwWindow, [](GLFWwindow* w, int) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Window);
    });
    glfwSetWindowCloseCallback(m_glfwWindow, [](GLFWwindow* w) {
        static_cast<ImGuiRenderer*>(glfwGetWindowUserPointer(w))->Invalidate(FrameReason::Window);
    });
#ifdef __EMSCRIPTEN__
    m_windowCallbackCount = 12;
#else
    m_windowCallbackCount = 13;
#endif
}

json ImGuiRenderer::GetPlatformDiagnostics() {
    json result = {{"windowCallbacks", m_windowCallbackCount.load()}};
#ifdef __EMSCRIPTEN__
    // All Wasm access runs on the browser thread, including diagnostics.
    result["animationCallbacks"] = m_browserFrame ? 1 : 0;
    result["deadlineTimers"] = m_browserTimer ? 1 : 0;
    result["visibilityListeners"] = m_visibilityListener ? 1 : 0;
    result["pendingScreenshots"] = 0;
#else
    result["animationCallbacks"] = result["deadlineTimers"] = result["visibilityListeners"] = 0;
    const std::lock_guard lock(m_screenshotMutex);
    result["pendingScreenshots"] = m_screenshotRequests.size();
#endif
    return result;
}

void ImGuiRenderer::ProcessWindowRequests() {
    std::optional<std::pair<int, int>> requested;
    {
        const std::lock_guard lock(m_windowRequestMutex);
        requested.swap(m_requestedSize);
    }
    if (requested) {
        m_sizeSuppressed = requested->first <= 0 || requested->second <= 0;
        // GLFW requires positive sizes. Zero suspends drawing until restoration.
        if (!m_sizeSuppressed) glfwSetWindowSize(m_glfwWindow, requested->first, requested->second);
    }
}
bool ImGuiRenderer::UpdateSurfaceAvailability() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_glfwWindow, &width, &height);
    bool available = !m_sizeSuppressed && width > 0 && height > 0;
#ifdef __EMSCRIPTEN__
    available &= !m_browserHidden;
#else
    available &= glfwGetWindowAttrib(m_glfwWindow, GLFW_ICONIFIED) == GLFW_FALSE;
#endif
    m_xframes->m_frameScheduler.SetRenderable(available);
    return available;
}

bool ImGuiRenderer::PrepareFrame(int& width, int& height) {
    if (!m_glfwWindow) return true;
    ProcessWindowRequests();
    if (!UpdateSurfaceAvailability()) return false;
    glfwGetWindowSize(m_glfwWindow, &width, &height);
    m_window_width = width;
    m_window_height = height;
    HandleScreenSizeChanged();
#ifdef __EMSCRIPTEN__
    ImGui_ImplWGPU_NewFrame();
#else
    ImGui_ImplOpenGL3_NewFrame();
#endif
    ImGui_ImplGlfw_NewFrame();
    const auto now = m_xframes->m_frameScheduler.Now();
    const float delta = m_previousFrameTime ? std::chrono::duration<float>(now - *m_previousFrameTime).count() : 1.0f / 60;
    // Preserve actual deadline intervals such as an 800-ms cursor blink. Resume
    // from unbounded idle/suspension caps the first interaction step at 100 ms;
    // active/deadline callbacks preserve elapsed time so long hover/cursor
    // deadlines do not drift. Scheduler deadlines use the same wall clock.
    m_imGuiCtx->IO.DeltaTime = m_resumingFromIdle ? std::clamp(delta, 0.00001f, 0.1f) : std::max(delta, 0.00001f);
    m_previousFrameTime = now;
    m_resumingFromIdle = false;
    return true;
}

void ImGuiRenderer::FinishFrame() {
    if (m_glfwWindow) UpdateImGuiActivity();
}
void ImGuiRenderer::UpdateImGuiActivity() {
    auto& g = *m_imGuiCtx;
    auto& io = g.IO;
    const auto now = m_xframes->m_frameScheduler.Now();
    const auto after = [&](float seconds) { return now + std::chrono::duration_cast<FrameScheduler::Time::duration>(std::chrono::duration<float>(seconds)); };
    bool layout = !g.InputEventsQueue.empty();
    for (const auto* window : g.Windows) if (window->Active) {
        layout |= window->AutoFitFramesX > 0 || window->AutoFitFramesY > 0 || window->HiddenFramesCannotSkipItems > 0;
        layout |= window->ScrollTarget.x < FLT_MAX || window->ScrollTarget.y < FLT_MAX;
    }
    // ImGui records stationary hover unlocking at the next NewFrame, before
    // updating MouseStationaryTimer. That transition needs one settling frame.
    layout |= g.HoverItemDelayId && g.MouseStationaryTimer >= g.Style.HoverStationaryDelay
        && g.HoverItemUnlockedStationaryId != g.HoverItemDelayId;
    m_imguiActivity[0].Set(layout);

    bool heldMouse = false;
    for (bool down : io.MouseDown) heldMouse |= down;
    const bool dimTarget = ImGui::GetTopMostPopupModal() != nullptr || g.NavWindowingTarget != nullptr;
    const bool fading = dimTarget ? g.DimBgRatio < 1.0f : g.DimBgRatio > 0.0f;
    const bool interaction = (heldMouse && (g.ActiveId || g.DragDropActive || g.MovingWindow))
        || g.NavMoveSubmitted || g.NavMoveForwardToNextFrame || g.NavInitRequest
        || g.NavHighlightActivatedTimer > 0 || fading
        || (g.NavWindowingTarget && g.NavWindowingHighlightAlpha < 1)
        || (!g.NavWindowingTarget && g.NavWindowingHighlightAlpha > 0);
    m_imguiActivity[1].Set(interaction);

    std::optional<FrameScheduler::Time> cursor;
    if (io.ConfigInputTextCursorBlink && g.InputTextState.ID && g.ActiveId == g.InputTextState.ID) {
        const float animation = g.InputTextState.CursorAnim;
        const float phase = animation < 0 ? animation : std::fmod(animation, 1.2f);
        cursor = after((phase < 0.8f ? 0.8f : 1.2f) - phase + 0.001f);
    }
    m_imguiActivity[2].Set(false, cursor);

    std::optional<FrameScheduler::Time> repeat;
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_Mouse_BEGIN; ++key) {
        if (key >= ImGuiKey_LeftCtrl && key <= ImGuiKey_RightSuper) continue;
        const auto* data = ImGui::GetKeyData(static_cast<ImGuiKey>(key));
        if (!data->Down || data->DownDuration < 0 || io.KeyRepeatRate <= 0) continue;
        const float delay = data->DownDuration < io.KeyRepeatDelay ? io.KeyRepeatDelay - data->DownDuration
            : io.KeyRepeatRate - std::fmod(data->DownDuration - io.KeyRepeatDelay, io.KeyRepeatRate);
        const auto deadline = after(std::max(delay, 0.001f));
        if (!repeat || deadline < *repeat) repeat = deadline;
    }
    m_imguiActivity[3].Set(false, repeat);

    std::optional<FrameScheduler::Time> hover;
    if (g.HoverItemDelayId || g.HoveredId) {
        // One-shot transitions, never permanent activity for a hovered item.
        const float elapsed = g.HoverItemDelayId ? g.HoverItemDelayTimer : g.HoveredIdTimer;
        for (float remaining : {g.Style.HoverDelayShort - elapsed,
                g.Style.HoverDelayNormal - elapsed,
                g.Style.HoverStationaryDelay - g.MouseStationaryTimer}) {
            if (remaining > 0) {
                const auto deadline = after(remaining + 0.001f);
                if (!hover || deadline < *hover) hover = deadline;
            }
        }
    }
    // Wheel ownership has an actual expiry; it does not need continuous redraw.
    if (g.WheelingWindow && g.WheelingWindowReleaseTimer > 0) {
        const auto deadline = after(g.WheelingWindowReleaseTimer + 0.001f);
        if (!hover || deadline < *hover) hover = deadline;
    }
    m_imguiActivity[4].Set(false, hover);
}

void ImGuiRenderer::DrawFrame() {
    if (!m_xframes->Render(m_window_width, m_window_height)) {
#ifndef __EMSCRIPTEN__
        FailScreenshotRequests("Renderer is unable to construct a frame");
#endif
        return;
    }
    m_xframes->FlushResourceEvents();
    if (PerformRendering()) {
        m_failedSubmissions = 0;
        m_xframes->CompleteDiagnosticsFrame();
#ifndef __EMSCRIPTEN__
        FlushScreenshotRequests();
        glfwSwapBuffers(m_glfwWindow);
#endif
    } else {
        m_xframes->AbandonFrame();
        if (m_xframes->m_frameScheduler.GetStatus() != FrameScheduler::Status::Running) {
#ifndef __EMSCRIPTEN__
            StopScreenshotRequests("Renderer backend failed");
#endif
            return;
        }
        // Three delayed recoverable retries, never a periodic rescue render.
        const int delays[] = {16, 64, 250};
        if (m_failedSubmissions < std::size(delays)) {
            m_xframes->m_frameScheduler.DeferUntil(m_xframes->m_frameScheduler.Now()
                + std::chrono::milliseconds(delays[m_failedSubmissions++]));
#ifdef __EMSCRIPTEN__
            m_wgpu_surface_width = m_wgpu_surface_height = 0;
#endif
        } else {
            fprintf(stderr, "Surface submission retry limit exhausted\n");
            m_xframes->m_frameScheduler.GetSource().FailBackend();
        }
    }
}

#ifdef __EMSCRIPTEN__
void ImGuiRenderer::RequestBrowserFrame() {
    if (!m_loopRunning || m_browserHidden || m_browserFrame) return;
    if (m_browserTimer) emscripten_clear_timeout(std::exchange(m_browserTimer, 0));
    m_browserFrame = emscripten_request_animation_frame([](double, void* data) -> bool {
        auto* self = static_cast<ImGuiRenderer*>(data);
        self->m_browserFrame = 0;
        self->BrowserFrame();
        return false;
    }, this);
}
void ImGuiRenderer::ScheduleBrowserNext() {
    const auto next = m_xframes->m_frameScheduler.PlanNext();
    if (!m_loopRunning || m_browserHidden || next.terminal || m_browserFrame) return;
    if (next.render) RequestBrowserFrame();
    else if (next.deadline) {
        if (m_browserTimer) emscripten_clear_timeout(std::exchange(m_browserTimer, 0));
        const auto ms = std::chrono::duration<double, std::milli>(*next.deadline - m_xframes->m_frameScheduler.Now()).count();
        m_browserTimer = emscripten_set_timeout([](void* data) {
            auto* self = static_cast<ImGuiRenderer*>(data);
            self->m_browserTimer = 0;
            self->RequestBrowserFrame();
        }, std::max(ms, 0.0), this);
    } else m_resumingFromIdle = true;
}
void ImGuiRenderer::BrowserFrame() {
    if (!m_loopRunning) return;
    glfwPollEvents();
    ProcessWindowRequests();
    UpdateSurfaceAvailability();
    if (m_xframes->m_frameScheduler.TakeOpportunity().render) DrawFrame();
    ScheduleBrowserNext();
}
void ImGuiRenderer::StopBrowserScheduling() {
    if (m_browserFrame) emscripten_cancel_animation_frame(std::exchange(m_browserFrame, 0));
    if (m_browserTimer) emscripten_clear_timeout(std::exchange(m_browserTimer, 0));
    emscripten_set_visibilitychange_callback(nullptr, false, nullptr);
    m_visibilityListener = false;
}
#endif
