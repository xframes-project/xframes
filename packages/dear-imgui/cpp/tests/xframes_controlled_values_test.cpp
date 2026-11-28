// Controlled values (ADR 0011 in xframes-core): eventsApplied, and the widgets that show a user edit until the application has seen it.
#include "xframes_test_support.h"

// Controlled values: a commit's eventsApplied names the last widget event the
// application had applied. An element edited after it keeps what the user typed;
// any other shows the committed value, even when the commit does not patch it.
namespace {
json Applied(json wire, uint64_t events) { wire["eventsApplied"] = std::to_string(events); return wire; }
}
TEST_F(XFramesTest, ControlledValuesAreAdvertisedAndEventsAreNumbered) {
    const auto features = xf->GetCommitState()["commitFeatures"];
    EXPECT_NE(std::find(features.begin(), features.end(), "controlledValues"), features.end());
    EXPECT_EQ(xf->LastEventSequence(), 0u);
    EXPECT_EQ(xf->NoteWidgetEvent(0), 1u);
    EXPECT_EQ(xf->NoteWidgetEvent(7), 2u);
    EXPECT_EQ(xf->LastEventSequence(), 2u);
    auto leadingZero = Wire(); leadingZero["eventsApplied"] = "01";
    Reject(leadingZero, "invalid_field");
    auto number = Wire(); number["eventsApplied"] = 3;
    Reject(number, "invalid_field");
}
TEST_F(XFramesTest, ControlledValueIsShownWithoutUnacknowledgedEdits) {
    const auto text = [&] { return static_cast<InputText*>(ElementAt(1))->m_value; };
    auto created = xf->ApplyCommit(Applied(Wire({{0, {1}}, {1, {}}}, {Create(1, "input-text", {{"value", "a"}})}), 0).dump());
    ASSERT_EQ(created.status, "applied") << created.ToJson();
    EXPECT_EQ(text(), "a");
    // Nobody edited it: a patch is shown at once.
    ASSERT_EQ(xf->ApplyCommit(Applied(Wire({{0, {1}}, {1, {}}}, {Patch(1, {{"value", "x"}})}), 0).dump()).status, "applied");
    EXPECT_EQ(text(), "x");
    EXPECT_EQ(ElementAt(1)->m_committedControlled, (json{{"value", "x"}}));
}
TEST_F(XFramesTest, EditAfterEventsAppliedKeepsTheUsersValueUntilAcknowledged) {
    const auto input = [&] { return static_cast<InputText*>(ElementAt(1)); };
    ASSERT_EQ(xf->ApplyCommit(Applied(Wire({{0, {1}}, {1, {}}}, {Create(1, "input-text", {{"value", ""}})}), 0).dump()).status, "applied");
    // The user types "a" (event 1), then "ab" (event 2). A commit built after event 1 crosses event 2.
    input()->SetValue("a"); const auto first = xf->NoteWidgetEvent(1);
    input()->SetValue("ab"); const auto second = xf->NoteWidgetEvent(1);
    ASSERT_EQ(xf->ApplyCommit(Applied(Wire({{0, {1}}, {1, {}}}, {Patch(1, {{"value", "a"}})}), first).dump()).status, "applied");
    EXPECT_EQ(input()->m_value, "ab");
    EXPECT_EQ(input()->m_committedControlled["value"], "a");
    // The model rejected "ab" and kept "a": a commit acknowledging event 2, with no patch, restores it.
    ASSERT_EQ(xf->ApplyCommit(Applied(Wire({{0, {1}}, {1, {}}}), second).dump()).status, "applied");
    EXPECT_EQ(input()->m_value, "a");
    EXPECT_TRUE(input()->m_reloadEditBuffer);
    // Settled: with no new edit, a commit that does not touch it leaves it alone.
    input()->m_value = "untouched";
    ASSERT_EQ(xf->ApplyCommit(Applied(Wire({{0, {1}}, {1, {}}}), second).dump()).status, "applied");
    EXPECT_EQ(input()->m_value, "untouched");
}
TEST_F(XFramesTest, ControlledValuesWithoutEventsAppliedAreIgnoredAsBefore) {
    Publish({{0, {1}}, {1, {}}}, {Create(1, "input-text", {{"value", "a"}})});
    EXPECT_EQ(static_cast<InputText*>(ElementAt(1))->m_value, "");
    Publish({{0, {1}}, {1, {}}}, {Patch(1, {{"value", "b"}})});
    EXPECT_EQ(static_cast<InputText*>(ElementAt(1))->m_value, "");
    EXPECT_TRUE(ElementAt(1)->m_committedControlled.empty());
}
TEST_F(XFramesTest, DestroyingAnElementForgetsItsUnacknowledgedEdit) {
    ASSERT_EQ(xf->ApplyCommit(Applied(Wire({{0, {1}}, {1, {}}}, {Create(1, "input-text", {{"value", "a"}})}), 0).dump()).status, "applied");
    xf->NoteWidgetEvent(1);
    ASSERT_EQ(xf->ApplyCommit(Applied(Wire(), 0).dump()).status, "applied");
    // The ID comes back for a new input: the old edit does not hold its value back.
    ASSERT_EQ(xf->ApplyCommit(Applied(Wire({{0, {1}}, {1, {}}}, {Create(1, "input-text", {{"value", "new"}})}), 0).dump()).status, "applied");
    EXPECT_EQ(static_cast<InputText*>(ElementAt(1))->m_value, "new");
}

namespace {
XFrames* g_clickedView = nullptr;
std::vector<uint64_t> g_clickSequences;
// As the C library queues a click: the event is numbered for the element it names.
void NoteClick(int id) { g_clickSequences.push_back(g_clickedView->NoteWidgetEvent(id)); }
}

class TreeNodeFrameTest : public XFramesQueueTest {
protected:
    // The tree node is 2, the only child of the root; its child is the text 3.
    void Commit(std::initializer_list<json> operations, std::optional<uint64_t> events) {
        auto wire = Wire({{0, {1}}, {1, {2}}, {2, {3}}, {3, {}}}, operations);
        if (events) wire = Applied(wire, *events);
        const auto result = xf->ApplyCommit(wire.dump());
        ASSERT_EQ(result.status, "applied") << result.ToJson();
    }
    void CreateNode(json props, std::optional<uint64_t> events) {
        Commit({Create(2, "tree-node", props), Create(3, "unformatted-text", {{"text", "child"}})}, events);
    }
    TreeNode* Node() { return static_cast<TreeNode*>(ElementAt(2)); }
    void Frames(int count = 3) {
        for (int i = 0; i < count; ++i) { xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative); ASSERT_TRUE(xf->Render(900, 700)); }
    }
    bool ChildShown() { return YGNodeStyleGetDisplay(ElementAt(3)->m_layoutNode->m_node) != YGDisplayNone; }
    // Presses and releases the mouse on the node's header line, where ImGui draws it.
    void ClickHeader() {
        ImVec2 at(0, 0);
        for (int id : {1, 2}) {
            at.x += YGNodeLayoutGetLeft(ElementAt(id)->m_layoutNode->m_node);
            at.y += YGNodeLayoutGetTop(ElementAt(id)->m_layoutNode->m_node);
        }
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(at.x + 40, at.y + ImGui::GetFontSize() / 2); Frames(1);
        io.AddMouseButtonEvent(0, true); Frames(1);
        io.AddMouseButtonEvent(0, false); Frames(1);
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); Frames();
    }
    void SetUp() override {
        XFramesQueueTest::SetUp();
        g_clickedView = xf.get(); g_clickSequences.clear();
        xf->m_onClick = NoteClick;
    }
};

// A controlled tree node (ADR 0011 in xframes-core): a click is the user's edit. The node shows it
// toggled, open and selected, until a commit says the application has seen the click. It then
// shows the committed values, whether the application took the edit or not.
TEST_F(TreeNodeFrameTest, ControlledTreeNodeShowsAClickUntilTheApplicationHasSeenIt) {
    CreateNode({{"itemId", "a"}, {"label", "Node"}, {"selectable", true}, {"open", false}, {"selected", false}}, 0);
    Frames();
    EXPECT_FALSE(ChildShown()) << "a closed node hides its child";
    ClickHeader();
    ASSERT_EQ(g_clickSequences.size(), 1u) << "the click is reported once";
    EXPECT_EQ(Node()->m_open, true) << "the click shows the node open at once";
    EXPECT_EQ(Node()->m_selected, true) << "and selected";
    EXPECT_TRUE(ChildShown());

    // A commit built before the application saw the click does not undo it.
    Commit({Patch(2, {{"label", "Renamed"}, {"open", false}, {"selected", false}})}, g_clickSequences[0] - 1);
    Frames();
    EXPECT_EQ(Node()->m_open, true);
    EXPECT_EQ(Node()->m_selected, true);
    EXPECT_TRUE(ChildShown());

    // The application takes the click.
    Commit({Patch(2, {{"open", true}, {"selected", true}})}, g_clickSequences[0]);
    Frames();
    EXPECT_EQ(Node()->m_open, true);
    EXPECT_TRUE(ChildShown());

    // A second click, which the application rejects: shown closed until the application has
    // seen it, then open again although the commit carries no patch.
    ClickHeader();
    ASSERT_EQ(g_clickSequences.size(), 2u);
    EXPECT_EQ(Node()->m_open, false);
    EXPECT_EQ(Node()->m_selected, false);
    EXPECT_FALSE(ChildShown());
    Commit({}, g_clickSequences[1]);
    Frames();
    EXPECT_EQ(Node()->m_open, true) << "a rejected click is undone once the application has seen it";
    EXPECT_EQ(Node()->m_selected, true);
    EXPECT_TRUE(ChildShown());

    // With no edit pending, the application's change is shown at once.
    Commit({Patch(2, {{"open", false}, {"selected", false}})}, g_clickSequences[1]);
    EXPECT_EQ(Node()->m_open, false);
    EXPECT_EQ(Node()->m_selected, false);
}

// A node that is not selectable is never shown selected, by a click or by the application.
TEST_F(TreeNodeFrameTest, ControlledTreeNodeThatIsNotSelectableIsNeverSelected) {
    CreateNode({{"itemId", "a"}, {"label", "Node"}, {"selectable", false}, {"open", false}, {"selected", false}}, 0);
    Frames();
    ClickHeader();
    ASSERT_EQ(g_clickSequences.size(), 1u);
    EXPECT_EQ(Node()->m_open, true);
    EXPECT_NE(Node()->m_selected, std::optional<bool>(true));
    Commit({Patch(2, {{"selected", true}})}, g_clickSequences[0]);
    EXPECT_NE(Node()->m_selected, std::optional<bool>(true));
}

// A patch of leaf turns an open node into a leaf, which shows nothing below it, and back.
TEST_F(TreeNodeFrameTest, PatchingLeafTurnsANodeIntoALeafAndBack) {
    CreateNode({{"itemId", "a"}, {"label", "Node"}, {"selectable", false}, {"open", true}, {"selected", false}, {"leaf", false}}, 0);
    Frames();
    EXPECT_TRUE(ChildShown());
    Commit({Patch(2, {{"leaf", true}})}, 0);
    Frames();
    EXPECT_TRUE(Node()->m_leaf);
    EXPECT_FALSE(ChildShown()) << "a leaf shows nothing below it";
    Commit({Patch(2, {{"leaf", false}})}, 0);
    Frames();
    EXPECT_TRUE(ChildShown());
}

// Without eventsApplied, open and selected props hold as before: a click only reports.
TEST_F(TreeNodeFrameTest, TreeNodePropsWithoutEventsAppliedHoldAsBefore) {
    CreateNode({{"itemId", "a"}, {"label", "Node"}, {"selectable", true}, {"open", false}, {"selected", false}}, std::nullopt);
    Frames();
    ClickHeader();
    ASSERT_EQ(g_clickSequences.size(), 1u);
    EXPECT_EQ(Node()->m_open, false);
    EXPECT_EQ(Node()->m_selected, false);
    EXPECT_FALSE(ChildShown());
    EXPECT_TRUE(Node()->m_committedControlled.empty());
}

namespace {
// As the C library queues a boolean change: the event is numbered for the element it names.
std::vector<uint64_t> g_toggleSequences;
void NoteToggle(int id, bool) { g_toggleSequences.push_back(g_clickedView->NoteWidgetEvent(id)); }
}

class CheckboxFrameTest : public XFramesQueueTest {
protected:
    // The checkbox is 2, the only child of the root.
    void Commit(std::initializer_list<json> operations, std::optional<uint64_t> events) {
        auto wire = Wire({{0, {1}}, {1, {2}}, {2, {}}}, operations);
        if (events) wire = Applied(wire, *events);
        const auto result = xf->ApplyCommit(wire.dump());
        ASSERT_EQ(result.status, "applied") << result.ToJson();
    }
    Checkbox* Box() { return static_cast<Checkbox*>(ElementAt(2)); }
    void Frames(int count = 3) {
        for (int i = 0; i < count; ++i) { xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative); ASSERT_TRUE(xf->Render(900, 700)); }
    }
    // Presses and releases the mouse on the checkbox's square, where ImGui draws it.
    void ClickBox() {
        ImVec2 at(0, 0);
        for (int id : {1, 2}) {
            at.x += YGNodeLayoutGetLeft(ElementAt(id)->m_layoutNode->m_node);
            at.y += YGNodeLayoutGetTop(ElementAt(id)->m_layoutNode->m_node);
        }
        const float half = ImGui::GetFrameHeight() / 2;
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(at.x + half, at.y + half); Frames(1);
        io.AddMouseButtonEvent(0, true); Frames(1);
        io.AddMouseButtonEvent(0, false); Frames(1);
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); Frames();
    }
    void SetUp() override {
        XFramesQueueTest::SetUp();
        g_clickedView = xf.get(); g_toggleSequences.clear();
        xf->m_onBooleanValueChange = NoteToggle;
    }
};

// A controlled checkbox (ADR 0011 in xframes-core): a click is the user's edit. The checkbox shows
// it toggled until a commit says the application has seen the click, and then shows the committed
// value, whether the application took the edit or not.
TEST_F(CheckboxFrameTest, ControlledCheckboxShowsAClickUntilTheApplicationHasSeenIt) {
    Commit({Create(2, "checkbox", {{"label", "Wrap"}, {"checked", false}})}, 0);
    Frames();
    EXPECT_FALSE(Box()->m_checked);
    ClickBox();
    ASSERT_EQ(g_toggleSequences.size(), 1u) << "the click is reported once";
    EXPECT_TRUE(Box()->m_checked) << "the click shows the checkbox checked at once";

    // A commit built before the application saw the click does not undo it.
    Commit({Patch(2, {{"label", "Wrap lines"}, {"checked", false}})}, g_toggleSequences[0] - 1);
    Frames();
    EXPECT_TRUE(Box()->m_checked);
    EXPECT_EQ(Box()->m_label, "Wrap lines");

    // The application takes the click.
    Commit({Patch(2, {{"checked", true}})}, g_toggleSequences[0]);
    Frames();
    EXPECT_TRUE(Box()->m_checked);

    // A second click, which the application rejects: shown unchecked until the application has
    // seen it, then checked again although the commit carries no patch.
    ClickBox();
    ASSERT_EQ(g_toggleSequences.size(), 2u);
    EXPECT_FALSE(Box()->m_checked);
    Commit({}, g_toggleSequences[1]);
    Frames();
    EXPECT_TRUE(Box()->m_checked) << "a rejected click is undone once the application has seen it";

    // With no edit pending, the application's change is shown at once.
    Commit({Patch(2, {{"checked", false}})}, g_toggleSequences[1]);
    EXPECT_FALSE(Box()->m_checked);
}

// Without eventsApplied, checked set by props holds as a plain property: created and patched.
TEST_F(CheckboxFrameTest, CheckboxCheckedWithoutEventsAppliedIsAProperty) {
    Commit({Create(2, "checkbox", {{"label", "Wrap"}, {"checked", true}})}, std::nullopt);
    EXPECT_TRUE(Box()->m_checked);
    Commit({Patch(2, {{"checked", false}})}, std::nullopt);
    EXPECT_FALSE(Box()->m_checked);
    EXPECT_TRUE(Box()->m_committedControlled.empty());
    // defaultChecked still seeds a checkbox created without checked.
    ASSERT_EQ(xf->ApplyCommit(Wire({{0, {1}}, {1, {}}}).dump()).status, "applied");
    Commit({Create(2, "checkbox", {{"label", "Other"}, {"defaultChecked", true}})}, std::nullopt);
    EXPECT_TRUE(Box()->m_checked);
}

namespace {
// As the C library queues a combo change: the event is numbered for the element it names.
std::vector<uint64_t> g_pickSequences;
std::vector<int> g_pickedIndexes;
void NotePick(int id, int index) { g_pickSequences.push_back(g_clickedView->NoteWidgetEvent(id)); g_pickedIndexes.push_back(index); }
}

class ComboFrameTest : public XFramesQueueTest {
protected:
    // The combo is 2, the only child of the root.
    void Commit(std::initializer_list<json> operations, std::optional<uint64_t> events) {
        auto wire = Wire({{0, {1}}, {1, {2}}, {2, {}}}, operations);
        if (events) wire = Applied(wire, *events);
        const auto result = xf->ApplyCommit(wire.dump());
        ASSERT_EQ(result.status, "applied") << result.ToJson();
    }
    Combo* Box() { return static_cast<Combo*>(ElementAt(2)); }
    void Frames(int count = 3) {
        for (int i = 0; i < count; ++i) { xf->m_frameScheduler.Invalidate(xframes::FrameReason::Imperative); ASSERT_TRUE(xf->Render(900, 700)); }
    }
    void Click(ImVec2 at) {
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(at.x, at.y); Frames(1);
        io.AddMouseButtonEvent(0, true); Frames(1);
        io.AddMouseButtonEvent(0, false); Frames(1);
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); Frames();
    }
    // Opens the combo with a click on its frame, then clicks option n where ImGui draws it in the
    // combo's popup: one line per option, the item spacing between them.
    void Pick(int n) {
        ImVec2 at(0, 0);
        for (int id : {1, 2}) {
            at.x += YGNodeLayoutGetLeft(ElementAt(id)->m_layoutNode->m_node);
            at.y += YGNodeLayoutGetTop(ElementAt(id)->m_layoutNode->m_node);
        }
        Click(ImVec2(at.x + 10, at.y + ImGui::GetFrameHeight() / 2));
        const ImGuiContext& g = *ImGui::GetCurrentContext();
        ASSERT_FALSE(g.OpenPopupStack.empty()) << "the click opens the combo";
        const ImGuiWindow* popup = g.OpenPopupStack.back().Window;
        ASSERT_NE(popup, nullptr);
        const auto start = popup->DC.CursorStartPos;
        Click(ImVec2(start.x + 5, start.y + n * (g.FontSize + g.Style.ItemSpacing.y) + g.FontSize / 2));
        EXPECT_TRUE(g.OpenPopupStack.empty()) << "picking an option closes the combo";
    }
    void SetUp() override {
        XFramesQueueTest::SetUp();
        g_clickedView = xf.get(); g_pickSequences.clear(); g_pickedIndexes.clear();
        xf->m_onComboChange = NotePick;
    }
};

// A controlled combo (ADR 0011 in xframes-core): picking an option is the user's edit. The combo
// shows it until a commit says the application has seen the pick, and then shows the committed
// selection, whether the application took the pick or not.
TEST_F(ComboFrameTest, ControlledComboShowsAPickUntilTheApplicationHasSeenIt) {
    const json options = {"Low", "Medium", "High"};
    Commit({Create(2, "combo", {{"placeholder", "Quality"}, {"options", options}, {"selectedIndex", -1}})}, 0);
    Frames();
    EXPECT_EQ(Box()->m_selectedIndex, -1);
    Pick(1);
    ASSERT_EQ(g_pickSequences.size(), 1u) << "the pick is reported once";
    EXPECT_EQ(g_pickedIndexes[0], 1);
    EXPECT_EQ(Box()->m_selectedIndex, 1) << "the pick is shown at once";

    // A commit built before the application saw the pick does not undo it.
    Commit({Patch(2, {{"placeholder", "Detail"}, {"selectedIndex", -1}})}, g_pickSequences[0] - 1);
    Frames();
    EXPECT_EQ(Box()->m_selectedIndex, 1);
    EXPECT_EQ(Box()->m_placeholder, "Detail");

    // The application takes the pick.
    Commit({Patch(2, {{"selectedIndex", 1}})}, g_pickSequences[0]);
    Frames();
    EXPECT_EQ(Box()->m_selectedIndex, 1);

    // A second pick, which the application rejects: shown until the application has seen it, then
    // the committed selection again although the commit carries no patch.
    Pick(2);
    ASSERT_EQ(g_pickSequences.size(), 2u);
    EXPECT_EQ(g_pickedIndexes[1], 2);
    EXPECT_EQ(Box()->m_selectedIndex, 2);
    Commit({}, g_pickSequences[1]);
    Frames();
    EXPECT_EQ(Box()->m_selectedIndex, 1) << "a rejected pick is undone once the application has seen it";

    // With no edit pending, the application's change is shown at once, back to the placeholder too.
    Commit({Patch(2, {{"selectedIndex", 0}})}, g_pickSequences[1]);
    EXPECT_EQ(Box()->m_selectedIndex, 0);
    Commit({Patch(2, {{"selectedIndex", -1}})}, g_pickSequences[1]);
    EXPECT_EQ(Box()->m_selectedIndex, -1);
}

// Without eventsApplied, selectedIndex set by props holds as a plain property: created and patched.
TEST_F(ComboFrameTest, ComboSelectedIndexWithoutEventsAppliedIsAProperty) {
    const json options = {"Low", "Medium", "High"};
    Commit({Create(2, "combo", {{"options", options}, {"selectedIndex", 2}})}, std::nullopt);
    EXPECT_EQ(Box()->m_selectedIndex, 2);
    Commit({Patch(2, {{"selectedIndex", -1}})}, std::nullopt);
    EXPECT_EQ(Box()->m_selectedIndex, -1);
    EXPECT_TRUE(Box()->m_committedControlled.empty());
    // initialSelectedIndex still seeds a combo created without selectedIndex.
    ASSERT_EQ(xf->ApplyCommit(Wire({{0, {1}}, {1, {}}}).dump()).status, "applied");
    Commit({Create(2, "combo", {{"options", options}, {"initialSelectedIndex", 1}})}, std::nullopt);
    EXPECT_EQ(Box()->m_selectedIndex, 1);
}

// selectedIndex is an option's index, or -1 for none.
TEST_F(XFramesQueueTest, ComboSelectedIndexMustBeAnIndexOrNone) {
    for (const json bad : {json(-2), json(1.5), json("1"), json(true)}) {
        SCOPED_TRACE(bad.dump());
        auto wire = Wire({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "combo", {{"options", {"a"}}, {"selectedIndex", bad}})});
        EXPECT_EQ(xf->ApplyCommit(wire.dump()).status, "rejected");
    }
}
