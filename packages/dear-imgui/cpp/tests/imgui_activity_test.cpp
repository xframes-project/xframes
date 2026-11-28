// The frame scheduler's activity deadlines, driven by real ImGui hover, focus and key repeat.
#include "xframes_test_support.h"

namespace {
class ActivityProbeRenderer final : public ImPlotRenderer {
public:
    using ImPlotRenderer::ImPlotRenderer;
    void RegisterActivity() {
        const xframes::FrameReason reasons[] = {xframes::FrameReason::Layout, xframes::FrameReason::Interaction,
            xframes::FrameReason::Cursor, xframes::FrameReason::KeyRepeat, xframes::FrameReason::Hover};
        for (size_t i = 0; i < std::size(reasons); ++i) m_imguiActivity[i] = m_xframes->m_frameScheduler.Register(reasons[i]);
    }
    void ObserveActivity() { UpdateImGuiActivity(); }
};
}
class ImGuiActivityTest : public ::testing::Test {
protected:
    std::unique_ptr<XFrames> xf;
    std::unique_ptr<ActivityProbeRenderer> renderer;
    void SetUp() override {
        xf = std::make_unique<XFrames>("activity-test", std::nullopt);
        std::string fonts = "{}";
        renderer = std::make_unique<ActivityProbeRenderer>(xf.get(), "activity-test", "activity-test", fonts, std::nullopt);
        renderer->RegisterActivity();
        auto& io = ImGui::GetIO(); io.DisplaySize = ImVec2(900, 700);
        io.FontDefault = io.Fonts->AddFontDefault();
        unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.IniFilename = nullptr;
    }
    void TearDown() override {
        xf.reset(); ImPlot::DestroyContext(); ImGui::DestroyContext(renderer->m_imGuiCtx); renderer.reset();
    }
    void Frame(const std::function<void()>& draw, float delta = 0.1f) {
        ImGui::GetIO().DeltaTime = delta;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2(900, 700));
        ImGui::Begin("activity-test", nullptr, ImGuiWindowFlags_NoTitleBar);
        draw();
        ImGui::End(); ImGui::Render();
        renderer->ObserveActivity();
        const auto frame = xf->m_frameScheduler.Capture(0);
        ASSERT_TRUE(frame); ASSERT_TRUE(xf->m_frameScheduler.Complete(*frame));
    }
    json State() { return xf->m_frameScheduler.GetState(); }
};

TEST_F(ImGuiActivityTest, RealHoveredButtonHasFiniteDeadlinesAndThenSettles) {
    const auto button = [] { ImGui::Button("Button", ImVec2(120, 30)); };
    Frame(button); Frame(button);
    ImGui::GetIO().AddMousePosEvent(30, 20);
    Frame(button);
    ASSERT_NE(ImGui::GetCurrentContext()->HoveredId, 0);
    EXPECT_GT(State()["deadlines"].get<int>(), 0);
    for (int i = 0; i < 8; ++i) Frame(button);
    EXPECT_EQ(State()["activeOwners"], 0);
    EXPECT_EQ(State()["deadlines"], 0);
    EXPECT_FALSE(xf->m_frameScheduler.PlanNext().render);
}

TEST_F(ImGuiActivityTest, FocusedInputBlinksByDeadlineWithoutContinuousFrames) {
    char text[64] = "text";
    bool focus = true;
    const auto input = [&] {
        if (focus) { ImGui::SetKeyboardFocusHere(); focus = false; }
        ImGui::InputText("Input", text, sizeof(text));
    };
    for (int i = 0; i < 4; ++i) Frame(input);
    const auto& g = *ImGui::GetCurrentContext();
    ASSERT_NE(g.InputTextState.ID, 0);
    ASSERT_EQ(g.ActiveId, g.InputTextState.ID);
    EXPECT_EQ(State()["reasons"]["cursor"]["deadlines"], 1);
    EXPECT_EQ(State()["activeOwners"], 0);
    Frame(input, 0.801f);
    EXPECT_EQ(State()["reasons"]["cursor"]["deadlines"], 1);
    ImGui::ClearActiveID();
    Frame(input);
    EXPECT_EQ(State()["reasons"]["cursor"]["deadlines"], 0);
}

TEST_F(ImGuiActivityTest, RealKeyRepeatDeadlineIsReleasedOnKeyUp) {
    const auto draw = [] { ImGui::Button("Button"); };
    Frame(draw); Frame(draw);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_A, true);
    Frame(draw);
    EXPECT_EQ(State()["reasons"]["keyRepeat"]["deadlines"], 1);
    Frame(draw, 0.4f);
    EXPECT_EQ(State()["reasons"]["keyRepeat"]["deadlines"], 1);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_A, false);
    Frame(draw);
    EXPECT_EQ(State()["reasons"]["keyRepeat"]["deadlines"], 0);
}
