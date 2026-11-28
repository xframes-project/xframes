// Layout on a real ImGui frame: containers place their children where Yoga says, and each widget is laid out at the size ImGui draws it.
#include "xframes_test_support.h"

TEST_F(XFramesQueueTest, InactiveTabHeadersDoNotExtendParentCursorWithoutAnItem) {
    Publish({{0, {1}}, {1, {2}}, {2, {3, 4, 5}}, {3, {6}}, {4, {}}, {5, {}}, {6, {}}}, {
        Create(2, "tab-bar", {{"style", {{"flex", 1}, {"width", "100%"}}}}),
        Create(3, "tab-item", {{"label", "Connection"}, {"style", {{"flex", 1}, {"width", "100%"}}}}),
        Create(4, "tab-item", {{"label", "Map"}, {"style", {{"flex", 1}, {"width", "100%"}}}}),
        Create(5, "tab-item", {{"label", "Signals"}, {"style", {{"flex", 1}, {"width", "100%"}}}}),
        Create(6, "unformatted-text", {{"text", "Disconnected"}}),
    });
    auto& io = ImGui::GetIO();
    io.ConfigErrorRecoveryEnableAssert = false;
    io.ConfigErrorRecoveryEnableDebugLog = true;
    io.ConfigErrorRecoveryEnableTooltip = false;
    for (int i = 0; i < 4; ++i) {
        Frame();
        EXPECT_EQ(ImGui::GetCurrentContext()->ErrorCountCurrentFrame, 0);
    }
}

namespace { int g_lastClick = 0; }
// A group or text-wrap opens no ImGui window, but its children's Yoga positions are
// relative to it: they must be placed from the container's position, not from the
// enclosing window's origin. A button below a text, inside one, is where Yoga says.
TEST_F(XFramesQueueTest, InPlaceContainersPlaceChildrenFromTheirOwnPosition) {
    xf->m_onClick = [](int id) { g_lastClick = id; };
    // A node opens its own window and is the control: it places its children correctly already.
    for (const char* container : {"node", "group", "text-wrap"}) {
        SCOPED_TRACE(container);
        g_lastClick = 0;
        Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {4}}, {4, {}}}, {
            Create(2, "unformatted-text", {{"text", "Above"}}),
            Create(3, container, std::string(container) == "text-wrap" ? json{{"width", 400}} : json::object()),
            Create(4, "di-button", {{"label", "Inside"}})});
        const auto frame = [&] { xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative); ASSERT_TRUE(xf->Render(900, 700)); };
        for (int i = 0; i < 3; ++i) frame();
        ASSERT_GT(YGNodeLayoutGetTop(ElementAt(3)->m_layoutNode->m_node), 0.f) << "the container is below the text";
        ImVec2 at(0, 0);
        for (int id : {1, 3, 4}) {
            at.x += YGNodeLayoutGetLeft(ElementAt(id)->m_layoutNode->m_node);
            at.y += YGNodeLayoutGetTop(ElementAt(id)->m_layoutNode->m_node);
        }
        const auto* button = ElementAt(4)->m_layoutNode->m_node;
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(at.x + YGNodeLayoutGetWidth(button) / 2, at.y + YGNodeLayoutGetHeight(button) / 2);
        frame();
        io.AddMouseButtonEvent(0, true); frame();
        io.AddMouseButtonEvent(0, false); frame();
        EXPECT_EQ(g_lastClick, 4);
        Publish({{0, {1}}, {1, {}}});
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); frame();
    }
}

// Origins compose: a group inside a group adds both positions. A node inside a group opens a
// window of its own, so its children start again from that window's corner.
TEST_F(XFramesQueueTest, NestedContainersComposeLayoutOrigins) {
    xf->m_onClick = [](int id) { g_lastClick = id; };
    for (const char* inner : {"group", "node"}) {
        SCOPED_TRACE(inner);
        g_lastClick = 0;
        Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {5, 6}}, {5, {}}, {6, {4}}, {4, {}}}, {
            Create(2, "unformatted-text", {{"text", "Above"}}), Create(3, "group"),
            Create(5, "unformatted-text", {{"text", "Also above"}}), Create(6, inner),
            Create(4, "di-button", {{"label", "Inside"}})});
        const auto frame = [&] { xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative); ASSERT_TRUE(xf->Render(900, 700)); };
        for (int i = 0; i < 3; ++i) frame();
        ASSERT_GT(YGNodeLayoutGetTop(ElementAt(6)->m_layoutNode->m_node), 0.f) << "the inner container is below the inner text";
        ImVec2 at(0, 0);
        for (int id : {1, 3, 6, 4}) {
            at.x += YGNodeLayoutGetLeft(ElementAt(id)->m_layoutNode->m_node);
            at.y += YGNodeLayoutGetTop(ElementAt(id)->m_layoutNode->m_node);
        }
        const auto* button = ElementAt(4)->m_layoutNode->m_node;
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(at.x + YGNodeLayoutGetWidth(button) / 2, at.y + YGNodeLayoutGetHeight(button) / 2);
        frame();
        io.AddMouseButtonEvent(0, true); frame();
        io.AddMouseButtonEvent(0, false); frame();
        EXPECT_EQ(g_lastClick, 4);
        EXPECT_TRUE(xf->m_layoutOrigins.empty());
        Publish({{0, {1}}, {1, {}}});
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); frame();
    }
}

namespace {
std::vector<std::pair<int, bool>> g_booleanChanges;
// The centre of the close button of the `index`th tab of the only tab bar, where ImGui draws it.
ImVec2 TabCloseButtonCentre(int index) {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImGuiTabBar* bar = g.TabBars.GetByIndex(0);
    const ImGuiTabItem& tab = bar->Tabs[index];
    const float right = bar->BarRect.Min.x + tab.Offset - bar->ScrollingAnim + tab.Width;
    return ImVec2(right - g.Style.FramePadding.x - g.FontSize / 2, bar->BarRect.Min.y + g.Style.FramePadding.y + g.FontSize / 2);
}
}

// Closing a tab only reports it: the tab stays where it was, selected and showing its children,
// until the view drops it, as an input shows the model's value.
TEST_F(XFramesQueueTest, ClosedTabStaysUntilTheViewDropsIt) {
    g_booleanChanges.clear();
    xf->m_onBooleanValueChange = [](int id, bool value) { g_booleanChanges.emplace_back(id, value); };
    const json fill = {{"flex", 1}, {"width", "100%"}};
    Publish({{0, {1}}, {1, {2}}, {2, {3, 4}}, {3, {5}}, {4, {6}}, {5, {}}, {6, {}}}, {
        Create(2, "tab-bar", {{"style", fill}}),
        Create(3, "tab-item", {{"label", "One"}, {"closeable", true}, {"style", fill}}),
        Create(4, "tab-item", {{"label", "Two"}, {"closeable", true}, {"style", fill}}),
        Create(5, "unformatted-text", {{"text", "first"}}), Create(6, "unformatted-text", {{"text", "second"}})});
    const auto frame = [&] { xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative); ASSERT_TRUE(xf->Render(900, 700)); };
    const auto shown = [&](int id) { return YGNodeStyleGetDisplay(ElementAt(id)->m_layoutNode->m_node) != YGDisplayNone; };
    for (int i = 0; i < 3; ++i) frame();
    ASSERT_TRUE(shown(3)) << "the first tab starts selected";
    ASSERT_FALSE(shown(4));
    const ImGuiTabBar* bar = ImGui::GetCurrentContext()->TabBars.GetByIndex(0);
    ASSERT_EQ(bar->Tabs.Size, 2);
    const ImGuiID first = bar->Tabs[0].ID;

    auto& io = ImGui::GetIO();
    const auto close = TabCloseButtonCentre(0);
    io.AddMousePosEvent(close.x, close.y); frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame();
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    for (int i = 0; i < 3; ++i) frame();
    EXPECT_EQ(g_booleanChanges, (std::vector<std::pair<int, bool>>{{3, false}})) << "a close is reported once";
    EXPECT_TRUE(shown(3)) << "the closed tab is still selected and shows its children";
    EXPECT_FALSE(shown(4));
    ASSERT_EQ(bar->Tabs.Size, 2);
    EXPECT_EQ(bar->Tabs[0].ID, first) << "the closed tab keeps its place";
    EXPECT_EQ(bar->SelectedTabId, first);

    // The view drops the tab: the other tab is selected.
    Publish({{0, {1}}, {1, {2}}, {2, {4}}, {4, {6}}, {6, {}}});
    for (int i = 0; i < 3; ++i) frame();
    EXPECT_TRUE(shown(4));
    EXPECT_EQ(g_booleanChanges.size(), 1u);
}

namespace {
// What ImGui itself gives one widget, drawn in a frame of its own: the item's rectangle, and the
// size it reserves for layout, which for a widget that draws to the edge of its window, such as a
// separator text, is its minimum.
struct Drawn { ImVec2 item; ImVec2 reserved; };
Drawn DrawnByImGui(const std::function<void()>& draw, std::optional<std::pair<ImGuiStyleVar, ImVec2>> styleVar = std::nullopt) {
    ImGui::NewFrame();
    ImGui::Begin("imgui-oracle");
    if (styleVar) ImGui::PushStyleVar(styleVar->first, styleVar->second);
    const auto* window = ImGui::GetCurrentWindow();
    const ImVec2 start = window->DC.CursorPos;
    draw();
    const Drawn drawn{ImGui::GetItemRectSize(), {window->DC.CursorMaxPos.x - start.x, window->DC.CursorMaxPos.y - start.y}};
    if (styleVar) ImGui::PopStyleVar();
    ImGui::End();
    ImGui::EndFrame();
    return drawn;
}

// A style that sets one vector style variable, sent as floats as a serializer that keeps floats does.
json StyleWithVar(ImGuiStyleVar var, ImVec2 value) {
    return {{"vars", {{std::to_string(var), json::array({double(value.x), double(value.y)})}}}};
}
}

// A commit's style variable may be any finite number or pair of them, and ImGui's style variables
// are floats. The widget must keep every value the commit accepts: a fractional or negative one, and
// a whole one that a serializer sends as a float.
TEST_F(XFramesQueueTest, WidgetKeepsEveryStyleVarValueACommitAccepts) {
    const auto var = [](ImGuiStyleVar key) { return std::to_string(key); };
    const json vars = {{var(ImGuiStyleVar_Alpha), 0.5}, {var(ImGuiStyleVar_FrameRounding), -2},
        {var(ImGuiStyleVar_FramePadding), json::array({11.0, 7.0})},
        {var(ImGuiStyleVar_ItemSpacing), json::array({-1, 4.5})}};
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "unformatted-text", {{"text", "Encrypt"}, {"style", {{"vars", vars}}}})});
    const auto* widget = dynamic_cast<StyledWidget*>(ElementAt(2));
    ASSERT_TRUE(widget && widget->m_style && widget->m_style.value()->maybeBase);
    const auto& parts = widget->m_style.value()->maybeBase.value();
    ASSERT_TRUE(parts.maybeStyleVars);
    const auto& kept = parts.maybeStyleVars.value();
    ASSERT_EQ(kept.size(), 4);
    EXPECT_FLOAT_EQ(std::get<float>(kept.at(ImGuiStyleVar_Alpha)), 0.5f);
    EXPECT_FLOAT_EQ(std::get<float>(kept.at(ImGuiStyleVar_FrameRounding)), -2.0f);
    const auto padding = std::get<ImVec2>(kept.at(ImGuiStyleVar_FramePadding));
    EXPECT_FLOAT_EQ(padding.x, 11.0f);
    EXPECT_FLOAT_EQ(padding.y, 7.0f);
    const auto spacing = std::get<ImVec2>(kept.at(ImGuiStyleVar_ItemSpacing));
    EXPECT_FLOAT_EQ(spacing.x, -1.0f);
    EXPECT_FLOAT_EQ(spacing.y, 4.5f);
}

// ImGui draws a bullet text with the bullet and twice the frame padding before its text, and an
// empty one as the bullet alone. Yoga must lay it out at that size, or in a row the next sibling
// is drawn over it.
TEST_F(XFramesQueueTest, BulletTextIsLaidOutAtTheSizeImGuiDrawsIt) {
    struct Case { const char* name; std::string text; std::optional<ImVec2> framePadding; };
    for (const auto& c : {Case{"text", "Encrypt a file", std::nullopt}, Case{"empty", "", std::nullopt},
                          Case{"its own frame padding", "Encrypt a file", ImVec2(11, 7)}}) {
        SCOPED_TRACE(c.name);
        json props = {{"text", c.text}};
        if (c.framePadding) props["style"] = StyleWithVar(ImGuiStyleVar_FramePadding, *c.framePadding);
        Publish({{0, {1}}, {1, {2}}, {2, {3}}, {3, {}}}, {
            Create(2, "node", {{"style", {{"flexDirection", "row"}}}}), Create(3, "bullet-text", props)});
        xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
        ASSERT_TRUE(xf->Render(900, 700));
        const auto* laidOut = ElementAt(3)->m_layoutNode->m_node;
        std::optional<std::pair<ImGuiStyleVar, ImVec2>> styleVar;
        if (c.framePadding) styleVar = {{ImGuiStyleVar_FramePadding, *c.framePadding}};
        const auto drawn = DrawnByImGui([&] { ImGui::BulletText("%s", c.text.c_str()); }, styleVar).item;
        // Yoga rounds layout to whole pixels.
        EXPECT_NEAR(YGNodeLayoutGetWidth(laidOut), drawn.x, 1.0f);
        EXPECT_NEAR(YGNodeLayoutGetHeight(laidOut), drawn.y, 1.0f);
        Publish({{0, {1}}, {1, {}}});
    }
}

// ImGui draws text containing line breaks one line high per line. Every text widget must be laid
// out that tall, or in a column the next sibling is drawn over its later lines.
TEST_F(XFramesQueueTest, MultiLineTextIsLaidOutAsTallAsImGuiDrawsIt) {
    const std::string text = "Encrypt\na file\n\nand sign it";
    struct Case { const char* type; std::function<void()> draw; };
    for (const auto& c : {Case{"unformatted-text", [&] { ImGui::TextUnformatted(text.c_str()); }},
                          Case{"disabled-text", [&] { ImGui::TextDisabled("%s", text.c_str()); }},
                          Case{"bullet-text", [&] { ImGui::BulletText("%s", text.c_str()); }}}) {
        SCOPED_TRACE(c.type);
        Publish({{0, {1}}, {1, {2}}, {2, {3}}, {3, {}}}, {
            Create(2, "node", {{"style", {{"flexDirection", "row"}}}}), Create(3, c.type, {{"text", text}})});
        xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
        ASSERT_TRUE(xf->Render(900, 700));
        const auto* laidOut = ElementAt(3)->m_layoutNode->m_node;
        const auto drawn = DrawnByImGui(c.draw).item;
        ASSERT_GT(drawn.y, ImGui::GetFontSize() * 3) << "the oracle draws four lines";
        EXPECT_NEAR(YGNodeLayoutGetWidth(laidOut), drawn.x, 1.0f);
        EXPECT_NEAR(YGNodeLayoutGetHeight(laidOut), drawn.y, 1.0f);
        Publish({{0, {1}}, {1, {}}});
    }
}

// Inside a text-wrap ImGui wraps text at the wrap position, which it gives as a position in the
// enclosing window, so the width it wraps at depends on where the text is drawn. The text learns it
// when it is drawn and is laid out again: from the next frame on it is as tall as ImGui draws it.
TEST_F(XFramesQueueTest, WrappedTextIsLaidOutAsTallAsImGuiDrawsIt) {
    const std::string text = "Encrypt a file with a key that only its recipient holds, then sign it";
    const float wrapAt = 200.0f, indent = 30.0f;
    struct Case { const char* type; std::function<void()> draw; };
    for (const auto& c : {Case{"unformatted-text", [&] { ImGui::TextUnformatted(text.c_str()); }},
                          Case{"disabled-text", [&] { ImGui::TextDisabled("%s", text.c_str()); }}}) {
        SCOPED_TRACE(c.type);
        Publish({{0, {1}}, {1, {2}}, {2, {3}}, {3, {4, 5}}, {4, {}}, {5, {}}}, {
            Create(2, "node", {{"style", {{"padding", {{"left", indent}}}}}}), Create(3, "text-wrap", {{"width", wrapAt}}),
            Create(4, c.type, {{"text", text}}), Create(5, "unformatted-text", {{"text", "Next"}})});
        for (int frame = 0; frame < 3; ++frame) {
            xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
            ASSERT_TRUE(xf->Render(900, 700));
        }
        const auto* laidOut = ElementAt(4)->m_layoutNode->m_node;
        // The text starts indent into the node's window, so ImGui wraps it that much short of wrapAt.
        EXPECT_EQ(static_cast<Text*>(ElementAt(4))->m_wrapWidth, wrapAt - indent);
        const auto drawn = DrawnByImGui([&] {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapAt - indent);
            c.draw();
            ImGui::PopTextWrapPos();
        }).item;
        ASSERT_GT(drawn.y, ImGui::GetFontSize() * 2) << "the oracle wraps the text";
        // Its width is not checked: a column stretches its children across.
        EXPECT_NEAR(YGNodeLayoutGetHeight(laidOut), drawn.y, 1.0f);
        EXPECT_NEAR(YGNodeLayoutGetTop(ElementAt(5)->m_layoutNode->m_node), drawn.y, 1.0f) << "the next sibling is below it";

        // Out of the text-wrap it is one line again.
        Publish({{0, {1}}, {1, {2}}, {2, {4}}, {4, {}}}, {});
        for (int frame = 0; frame < 3; ++frame) {
            xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
            ASSERT_TRUE(xf->Render(900, 700));
        }
        EXPECT_NEAR(YGNodeLayoutGetHeight(ElementAt(4)->m_layoutNode->m_node), ImGui::GetFontSize(), 1.0f);
        Publish({{0, {1}}, {1, {}}});
    }
}

// ImGui reserves a separator text its label and the separator text padding around it, and draws
// its line on to the edge of the window. Yoga lays it out as tall as that, at that minimum width
// where nothing stretches it, and across the width its parent gives it where something does.
TEST_F(XFramesQueueTest, SeparatorTextIsLaidOutAtTheSizeImGuiDrawsIt) {
    struct Case { const char* name; std::string label; std::optional<ImVec2> padding; };
    for (const auto& c : {Case{"label", "Settings", std::nullopt}, Case{"empty", "", std::nullopt},
                          Case{"two lines", "Network\nsettings", std::nullopt},
                          Case{"text after ## hidden", "Settings##network", std::nullopt},
                          Case{"its own padding", "Settings", ImVec2(11, 7)}}) {
        SCOPED_TRACE(c.name);
        json props = {{"label", c.label}};
        if (c.padding) props["style"] = StyleWithVar(ImGuiStyleVar_SeparatorTextPadding, *c.padding);
        std::optional<std::pair<ImGuiStyleVar, ImVec2>> styleVar;
        if (c.padding) styleVar = {{ImGuiStyleVar_SeparatorTextPadding, *c.padding}};
        const auto drawn = DrawnByImGui([&] { ImGui::SeparatorText(c.label.c_str()); }, styleVar);

        // In a row nothing stretches it across: its minimum width.
        Publish({{0, {1}}, {1, {2}}, {2, {3}}, {3, {}}}, {
            Create(2, "node", {{"style", {{"flexDirection", "row"}}}}), Create(3, "separator-text", props)});
        xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
        ASSERT_TRUE(xf->Render(900, 700));
        const auto* inRow = ElementAt(3)->m_layoutNode->m_node;
        EXPECT_NEAR(YGNodeLayoutGetWidth(inRow), drawn.reserved.x, 1.0f);
        EXPECT_NEAR(YGNodeLayoutGetHeight(inRow), drawn.item.y, 1.0f);
        EXPECT_NEAR(drawn.reserved.y, drawn.item.y, 1.0f) << "ImGui reserves the height it draws";
        Publish({{0, {1}}, {1, {}}});

        // In a column it is stretched across its parent.
        Publish({{0, {1}}, {1, {2}}, {2, {3}}, {3, {}}}, {
            Create(2, "node", {{"style", {{"width", 400}}}}), Create(3, "separator-text", props)});
        xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
        ASSERT_TRUE(xf->Render(900, 700));
        const auto* inColumn = ElementAt(3)->m_layoutNode->m_node;
        EXPECT_NEAR(YGNodeLayoutGetWidth(inColumn), 400.0f, 1.0f);
        EXPECT_NEAR(YGNodeLayoutGetHeight(inColumn), drawn.item.y, 1.0f);
        Publish({{0, {1}}, {1, {}}});
    }
}

// An item tooltip is drawn in a tooltip window of its own, over everything else, so it must take
// no space where it sits among its siblings: the sibling after it is laid out as if it were not
// there, gap included, in a column and in a row.
TEST_F(XFramesQueueTest, ItemTooltipTakesNoSpaceAmongItsSiblings) {
    struct Case { const char* name; json style; bool row; };
    for (const auto& c : {Case{"column", {{"gap", {{"row", 10}}}}, false},
                          Case{"row", {{"flexDirection", "row"}, {"gap", {{"column", 10}}}}, true}}) {
        SCOPED_TRACE(c.name);
        Publish({{0, {1}}, {1, {2}}, {2, {3, 4, 6}}, {3, {}}, {4, {5}}, {5, {}}, {6, {}}}, {
            Create(2, "node", {{"style", c.style}}), Create(3, "di-button", {{"label", "Save"}}), Create(4, "item-tooltip"),
            Create(5, "unformatted-text", {{"text", "Saves the file\nto disk"}}), Create(6, "unformatted-text", {{"text", "Next"}})});
        xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
        ASSERT_TRUE(xf->Render(900, 700));
        const auto* button = ElementAt(3)->m_layoutNode->m_node;
        const auto* next = ElementAt(6)->m_layoutNode->m_node;
        if (c.row) EXPECT_NEAR(YGNodeLayoutGetLeft(next), YGNodeLayoutGetLeft(button) + YGNodeLayoutGetWidth(button) + 10, 1.0f);
        else EXPECT_NEAR(YGNodeLayoutGetTop(next), YGNodeLayoutGetTop(button) + YGNodeLayoutGetHeight(button) + 10, 1.0f);
        Publish({{0, {1}}, {1, {}}});
    }
}

// ImGui shows an item tooltip while the item drawn before it is hovered. Its children are drawn
// inside the tooltip window from the window's padding, as ImGui draws a tooltip's contents.
TEST_F(XFramesQueueTest, ItemTooltipShowsItsChildrenWhileThePreviousItemIsHovered) {
    const std::string tip = "Saves the file";
    Publish({{0, {1}}, {1, {2}}, {2, {3, 4, 6}}, {3, {}}, {4, {5}}, {5, {}}, {6, {}}}, {
        Create(2, "node"), Create(3, "di-button", {{"label", "Save"}}), Create(4, "item-tooltip"),
        Create(5, "unformatted-text", {{"text", tip}}), Create(6, "di-button", {{"label", "Other"}})});
    const auto frames = [&](int count) {
        for (int i = 0; i < count; ++i) { xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative); ASSERT_TRUE(xf->Render(900, 700)); }
    };
    const auto tooltip = [&]() -> ImGuiWindow* {
        for (auto* window : ImGui::GetCurrentContext()->Windows)
            if ((window->Flags & ImGuiWindowFlags_Tooltip) && window->Active) return window;
        return nullptr;
    };
    const auto center = [&](int id) {
        ImVec2 at(0, 0);
        for (int each : {1, 2, id}) {
            at.x += YGNodeLayoutGetLeft(ElementAt(each)->m_layoutNode->m_node);
            at.y += YGNodeLayoutGetTop(ElementAt(each)->m_layoutNode->m_node);
        }
        const auto* node = ElementAt(id)->m_layoutNode->m_node;
        return ImVec2(at.x + YGNodeLayoutGetWidth(node) / 2, at.y + YGNodeLayoutGetHeight(node) / 2);
    };
    auto& io = ImGui::GetIO();
    frames(3);
    EXPECT_EQ(tooltip(), nullptr) << "nothing is hovered";
    const auto top = [&](int id) { return YGNodeLayoutGetTop(ElementAt(id)->m_layoutNode->m_node); };
    const auto otherTop = top(6);
    EXPECT_NEAR(otherTop, top(3) + YGNodeLayoutGetHeight(ElementAt(3)->m_layoutNode->m_node), 1.0f);

    io.AddMousePosEvent(center(3).x, center(3).y);
    frames(60);
    const auto* shown = tooltip();
    ASSERT_NE(shown, nullptr) << "hovering the button shows its tooltip";
    const auto size = ImGui::CalcTextSize(tip.c_str());
    EXPECT_NEAR(shown->DC.CursorMaxPos.x - shown->DC.CursorStartPos.x, size.x, 1.0f) << "the text starts at the window padding";
    EXPECT_NEAR(shown->DC.CursorMaxPos.y - shown->DC.CursorStartPos.y, size.y, 1.0f);
    EXPECT_NEAR(top(6), otherTop, 0.5f) << "a hover restyles the tooltip and it still takes no space";

    io.AddMousePosEvent(center(6).x, center(6).y);
    frames(60);
    EXPECT_EQ(tooltip(), nullptr) << "the tooltip follows the item before it, not the one after";

    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    frames(3);
    EXPECT_EQ(tooltip(), nullptr);
}

// ImGui draws a checkbox as its square, then its label after the item inner spacing, as tall as
// the frame. Yoga must lay it out at that size, or in a row the next sibling is drawn over its
// label. A label shows, and is sized, only up to a "##".
TEST_F(XFramesQueueTest, CheckboxIsLaidOutAtTheSizeImGuiDrawsIt) {
    struct Case { const char* name; std::string label; std::optional<std::pair<ImGuiStyleVar, ImVec2>> styleVar; };
    for (const auto& c : {Case{"label", "Wrap lines", std::nullopt}, Case{"empty", "", std::nullopt},
                          Case{"hidden id", "Wrap##wrap", std::nullopt},
                          Case{"its own frame padding", "Wrap lines", std::pair{ImGuiStyleVar_FramePadding, ImVec2(11, 7)}},
                          Case{"its own inner spacing", "Wrap lines", std::pair{ImGuiStyleVar_ItemInnerSpacing, ImVec2(13, 5)}}}) {
        SCOPED_TRACE(c.name);
        json props = {{"label", c.label}};
        if (c.styleVar) props["style"] = StyleWithVar(c.styleVar->first, c.styleVar->second);
        Publish({{0, {1}}, {1, {2}}, {2, {3}}, {3, {}}}, {
            Create(2, "node", {{"style", {{"flexDirection", "row"}}}}), Create(3, "checkbox", props)});
        xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
        ASSERT_TRUE(xf->Render(900, 700));
        const auto* laidOut = ElementAt(3)->m_layoutNode->m_node;
        bool checked = false;
        // The engine pushes the checkbox's id, so an empty label is not an empty id.
        const auto drawn = DrawnByImGui([&] { ImGui::PushID(3); ImGui::Checkbox(c.label.c_str(), &checked); ImGui::PopID(); }, c.styleVar).item;
        // Yoga rounds layout to whole pixels.
        EXPECT_NEAR(YGNodeLayoutGetWidth(laidOut), drawn.x, 1.0f);
        EXPECT_NEAR(YGNodeLayoutGetHeight(laidOut), drawn.y, 1.0f);
        Publish({{0, {1}}, {1, {}}});
    }
}

// A new label is measured again: the checkbox is laid out at the new label's width.
TEST_F(XFramesQueueTest, CheckboxIsMeasuredAgainWhenItsLabelChanges) {
    Publish({{0, {1}}, {1, {2}}, {2, {3}}, {3, {}}}, {
        Create(2, "node", {{"style", {{"flexDirection", "row"}}}}), Create(3, "checkbox", {{"label", "A"}})});
    const auto frame = [&] { xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative); ASSERT_TRUE(xf->Render(900, 700)); };
    frame();
    Publish({{0, {1}}, {1, {2}}, {2, {3}}, {3, {}}}, {Patch(3, {{"label", "A much longer label"}})});
    frame();
    bool checked = false;
    const auto drawn = DrawnByImGui([&] { ImGui::Checkbox("A much longer label", &checked); }).item;
    EXPECT_NEAR(YGNodeLayoutGetWidth(ElementAt(3)->m_layoutNode->m_node), drawn.x, 1.0f);
}

// A progress bar is drawn at the width it is laid out with, so it must be laid out with one: given
// no width, as in a row, it is ten font sizes wide, and the next sibling starts after it. Given a
// width, as in a column, it fills it. It is one frame tall, with its own frame padding. The oracle
// is the engine's own bar, drawn in a frame of its own.
TEST_F(XFramesQueueTest, ProgressBarIsLaidOutAtTheSizeItIsDrawn) {
    struct Case { const char* name; json containerStyle; std::optional<std::pair<ImGuiStyleVar, ImVec2>> styleVar; };
    float unstyledHeight = 0.0f;
    for (const auto& c : {Case{"in a row", {{"flexDirection", "row"}}, std::nullopt},
                          Case{"in a column of width 400", {{"width", 400}}, std::nullopt},
                          Case{"its own frame padding", {{"flexDirection", "row"}}, std::pair{ImGuiStyleVar_FramePadding, ImVec2(11, 7)}}}) {
        SCOPED_TRACE(c.name);
        json props = {{"fraction", 0.5}};
        if (c.styleVar) props["style"] = StyleWithVar(c.styleVar->first, c.styleVar->second);
        Publish({{0, {1}}, {1, {2}}, {2, {3, 4}}, {3, {}}, {4, {}}}, {
            Create(2, "node", {{"style", c.containerStyle}}), Create(3, "progress-bar", props),
            Create(4, "unformatted-text", {{"text", "Copying"}})});
        xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative);
        ASSERT_TRUE(xf->Render(900, 700));
        auto* bar = ElementAt(3);
        const auto* laidOut = bar->m_layoutNode->m_node;
        float fontSize = 0.0f;
        const auto drawn = DrawnByImGui([&] {
            fontSize = ImGui::GetFontSize();
            bar->PreRender(xf.get()); bar->Render(xf.get(), std::nullopt); bar->PostRender(xf.get());
        }).item;
        // Yoga rounds layout to whole pixels.
        EXPECT_NEAR(YGNodeLayoutGetWidth(laidOut), drawn.x, 1.0f) << "drawn at the width it is laid out with";
        EXPECT_NEAR(YGNodeLayoutGetHeight(laidOut), drawn.y, 1.0f) << "drawn at the height it is laid out with";
        if (std::string(c.name) == "in a column of width 400") {
            EXPECT_NEAR(YGNodeLayoutGetWidth(laidOut), 400.0f, 1.0f) << "a given width is filled";
        } else {
            EXPECT_NEAR(YGNodeLayoutGetWidth(laidOut), fontSize * 10, 1.0f) << "with no width given, ten font sizes";
            EXPECT_GE(YGNodeLayoutGetLeft(ElementAt(4)->m_layoutNode->m_node),
                      YGNodeLayoutGetLeft(laidOut) + YGNodeLayoutGetWidth(laidOut) - 0.5f) << "the next sibling starts after it";
        }
        if (!c.styleVar && unstyledHeight == 0.0f) unstyledHeight = YGNodeLayoutGetHeight(laidOut);
        if (c.styleVar) EXPECT_GT(YGNodeLayoutGetHeight(laidOut), unstyledHeight + 1.0f) << "its own padding is measured";
        Publish({{0, {1}}, {1, {}}});
    }
}
