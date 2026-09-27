#include <array>
#include <gtest/gtest.h>

#include "imgui_renderer.h"
#include "widget/button.h"
#include "widget/text.h"
#include "xframes.h"

namespace {
constexpr std::array names{"style", "hoverStyle", "activeStyle", "disabledStyle"};
constexpr std::array widgetMembers{&WidgetStyle::maybeBase, &WidgetStyle::maybeHover,
    &WidgetStyle::maybeActive, &WidgetStyle::maybeDisabled};
constexpr std::array elementMembers{&ElementStyle::maybeBase, &ElementStyle::maybeHover,
    &ElementStyle::maybeActive, &ElementStyle::maybeDisabled};
constexpr std::array states{ElementState_Base, ElementState_Hover, ElementState_Active, ElementState_Disabled};

// Real font lookup and ImGui fonts, without a desktop window or graphics driver.
class StyleTestRenderer : public ImGuiRenderer {
public:
    explicit StyleTestRenderer(XFrames* view) : ImGuiRenderer(view, "style-test", "style-test", "{}", std::nullopt) {
        for (int seed = 0; seed <= 7; ++seed) {
            ImFontConfig config;
            config.SizePixels = 14 + seed * 2;
            m_fontDefMap["test-font"][14 + seed * 2] = seed;
            m_loadedFonts.push_back(ImGui::GetIO().Fonts->AddFontDefault(&config));
        }
        SetFontDefault(0);
    }
};

json Family(unsigned seed) {
    return {{"font", {{"name", "test-font"}, {"size", 14u + seed * 2u}}},
        {"colors", {{std::to_string(ImGuiCol_Text), fmt::format("#{:02x}8090", seed)}}},
        {"vars", {{std::to_string(ImGuiStyleVar_FrameRounding), seed},
            {std::to_string(ImGuiStyleVar_FramePadding), json::array({seed, seed + 1})}}},
        {"width", 100u + seed}, {"backgroundColor", "#008080"}};
}

json Definition() {
    json props = {{"id", 1}, {"text", "Encrypt a file"}, {"label", "Encrypt"}};
    for (size_t i = 0; i < names.size(); ++i) props[names[i]] = Family(i + 1);
    return props;
}

void ExpectParts(const std::optional<WidgetStyleParts>& parts, unsigned seed) {
    ASSERT_TRUE(parts);
    EXPECT_EQ(parts->maybeFontIndex, seed);
    ASSERT_TRUE(parts->maybeColors);
    ASSERT_EQ(parts->maybeColors->size(), 1);
    const auto color = parts->maybeColors->at(ImGuiCol_Text);
    EXPECT_FLOAT_EQ(color.x, seed / 255.0f);
    EXPECT_FLOAT_EQ(color.y, 128 / 255.0f);
    EXPECT_FLOAT_EQ(color.z, 144 / 255.0f);
    EXPECT_FLOAT_EQ(color.w, 1);
    ASSERT_TRUE(parts->maybeStyleVars);
    ASSERT_EQ(parts->maybeStyleVars->size(), 2);
    EXPECT_FLOAT_EQ(std::get<float>(parts->maybeStyleVars->at(ImGuiStyleVar_FrameRounding)), seed);
    const auto padding = std::get<ImVec2>(parts->maybeStyleVars->at(ImGuiStyleVar_FramePadding));
    EXPECT_FLOAT_EQ(padding.x, seed);
    EXPECT_FLOAT_EQ(padding.y, seed + 1);
}
}

class StyledWidgetPatchTest : public ::testing::Test {
protected:
    std::unique_ptr<XFrames> view;
    std::unique_ptr<StyleTestRenderer> renderer;
    void SetUp() override {
        view = std::make_unique<XFrames>("style-test", std::nullopt);
        renderer = std::make_unique<StyleTestRenderer>(view.get());
        view->m_renderer = renderer.get();
    }
    void TearDown() override {
        view.reset();
        ImGui::DestroyContext(renderer->m_imGuiCtx);
        renderer.reset();
    }
    auto MakeText() {
        const auto def = Definition();
        auto widget = UnformattedText::makeWidget(def, StyledWidget::ExtractStyle(def, view.get()), view.get());
        widget->Init(def);
        return widget;
    }
    void ExpectFamily(const StyledWidget& widget, size_t family, unsigned seed) {
        ASSERT_TRUE(widget.m_style);
        ExpectParts((*widget.m_style.value()).*widgetMembers[family], seed);
        ASSERT_TRUE(widget.m_elementStyle);
        const auto& layout = widget.m_elementStyle.value().*elementMembers[family];
        ASSERT_TRUE(layout);
        EXPECT_EQ(layout->styleDef, Family(seed));
        ASSERT_TRUE(layout->backgroundColor);
        EXPECT_FLOAT_EQ(layout->backgroundColor->y, 128 / 255.0f);
    }
    void ExpectOtherFamilies(const StyledWidget& widget, size_t changed) {
        for (size_t i = 0; i < names.size(); ++i) if (i != changed) ExpectFamily(widget, i, i + 1);
    }
};

TEST_F(StyledWidgetPatchTest, TextOnlyPatchPreservesFontsColorsStyleVarsAndLayout) {
    auto widget = MakeText();
    auto* originalStyle = widget->m_style.value().get();
    widget->Patch({{"text", "Decrypt a file"}}, view.get());
    EXPECT_EQ(widget->m_text, "Decrypt a file");
    EXPECT_EQ(widget->m_style.value().get(), originalStyle);
    for (size_t i = 0; i < names.size(); ++i) ExpectFamily(*widget, i, i + 1);
    EXPECT_FLOAT_EQ(view->GetWidgetFontSize(widget.get()), 16);
    EXPECT_FLOAT_EQ(YGNodeStyleGetWidth(widget->m_layoutNode->m_node).value, 101);
}

TEST_F(StyledWidgetPatchTest, LabelOnlyPatchPreservesFontsColorsStyleVarsAndLayout) {
    const auto def = Definition();
    auto widget = Button::makeWidget(def, StyledWidget::ExtractStyle(def, view.get()), view.get());
    widget->Init(def);
    widget->Patch({{"label", "Working..."}}, view.get());
    EXPECT_EQ(widget->m_label, "Working...");
    for (size_t i = 0; i < names.size(); ++i) ExpectFamily(*widget, i, i + 1);
    EXPECT_FLOAT_EQ(view->GetFrameHeight(widget.get()), 20);
}

TEST_F(StyledWidgetPatchTest, NonStylePatchDoesNotCreateStyleOnAnUnstyledWidget) {
    StyledWidget widget(view.get(), 1);
    widget.Patch({{"label", "Working..."}}, view.get());
    EXPECT_FALSE(widget.m_style);
    EXPECT_FALSE(widget.m_elementStyle);
}

TEST_F(StyledWidgetPatchTest, PatchCanIntroduceStyleOnAnUnstyledWidget) {
    StyledWidget widget(view.get(), 1);
    widget.Patch({{"hoverStyle", Family(2)}}, view.get());
    ExpectFamily(widget, 1, 2);
    EXPECT_FALSE(widget.m_style.value()->maybeBase);
    EXPECT_EQ(widget.m_style.value()->GetCustomFontId(ElementState_Hover, view.get()), 2);
}

class StyledWidgetFamilyPatchTest : public StyledWidgetPatchTest, public ::testing::WithParamInterface<size_t> {};

TEST_P(StyledWidgetFamilyPatchTest, UpdatesOnlyTheExplicitFamily) {
    auto widget = MakeText();
    widget->Patch({{names[GetParam()], Family(7)}}, view.get());
    ExpectFamily(*widget, GetParam(), 7);
    ExpectOtherFamilies(*widget, GetParam());
}

TEST_P(StyledWidgetFamilyPatchTest, ReplacesRatherThanMergesWithinAFamily) {
    auto widget = MakeText();
    widget->Patch({{names[GetParam()], {{"colors", {{std::to_string(ImGuiCol_Text), "#ff0000"}}}}}}, view.get());
    const auto& parts = (*widget->m_style.value()).*widgetMembers[GetParam()];
    ASSERT_TRUE(parts);
    EXPECT_FALSE(parts->maybeFontIndex);
    EXPECT_FALSE(parts->maybeStyleVars);
    ASSERT_TRUE(parts->maybeColors);
    EXPECT_FLOAT_EQ(parts->maybeColors->at(ImGuiCol_Text).x, 1);
    ExpectOtherFamilies(*widget, GetParam());
}

TEST_P(StyledWidgetFamilyPatchTest, NullClearsOnlyTheExplicitFamily) {
    auto widget = MakeText();
    widget->Patch({{names[GetParam()], nullptr}}, view.get());
    EXPECT_FALSE((*widget->m_style.value()).*widgetMembers[GetParam()]);
    EXPECT_FALSE(widget->m_elementStyle.value().*elementMembers[GetParam()]);
    ExpectOtherFamilies(*widget, GetParam());
    // In particular, removing base must leave state-only font lookup usable.
    for (size_t i = 0; i < names.size(); ++i) {
        if (i != GetParam()) {
            EXPECT_TRUE(widget->m_style.value()->HasCustomFont(states[i], view.get()));
            EXPECT_EQ(widget->m_style.value()->GetCustomFontId(states[i], view.get()), i + 1);
            EXPECT_EQ(widget->m_style.value()->GetCustomColors(states[i]).size(), 1);
            EXPECT_EQ(widget->m_style.value()->GetCustomStyleVars(states[i]).size(), 2);
        }
    }
    if (GetParam() == 0) EXPECT_TRUE(YGFloatIsUndefined(YGNodeStyleGetWidth(widget->m_layoutNode->m_node).value));
}

TEST_P(StyledWidgetFamilyPatchTest, EmptyObjectReplacesOnlyTheExplicitFamily) {
    auto widget = MakeText();
    widget->Patch({{names[GetParam()], json::object()}}, view.get());
    const auto& parts = (*widget->m_style.value()).*widgetMembers[GetParam()];
    ASSERT_TRUE(parts);
    EXPECT_FALSE(parts->maybeFontIndex);
    EXPECT_FALSE(parts->maybeColors);
    EXPECT_FALSE(parts->maybeStyleVars);
    ExpectOtherFamilies(*widget, GetParam());
    if (GetParam() == 0) EXPECT_TRUE(YGFloatIsUndefined(YGNodeStyleGetWidth(widget->m_layoutNode->m_node).value));
}

TEST_F(StyledWidgetPatchTest, ClearingCurrentHoverLayoutRestoresBaseLayout) {
    auto widget = MakeText();
    widget->m_isHovered = true;
    widget->ApplyStyle();
    EXPECT_FLOAT_EQ(YGNodeStyleGetWidth(widget->m_layoutNode->m_node).value, 102);
    widget->Patch({{"hoverStyle", nullptr}}, view.get());
    EXPECT_FLOAT_EQ(YGNodeStyleGetWidth(widget->m_layoutNode->m_node).value, 101);
    widget->Patch({{"style", nullptr}}, view.get());
    EXPECT_TRUE(YGFloatIsUndefined(YGNodeStyleGetWidth(widget->m_layoutNode->m_node).value));
}

INSTANTIATE_TEST_SUITE_P(AllFamilies, StyledWidgetFamilyPatchTest, ::testing::Values(0, 1, 2, 3),
    [](const auto& info) { return names[info.param]; });
