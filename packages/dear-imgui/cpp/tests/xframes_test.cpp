#include <gtest/gtest.h>
#include <thread>
#include <algorithm>
#include <barrier>
#include <future>
#include <filesystem>
#include <latch>
#include <tuple>
#include <nlohmann/json.hpp>
#include <rpp/rpp.hpp>
#include "widget/styled_widget.h"
#include "xframes.h"
#include "element/element.h"
#include "implot_renderer.h"
#include "widget/table.h"
#include "widget/js_canvas.h"
#include "widget/lua_canvas.h"
#include "widget/janet_canvas.h"
#include "widget/map_view.h"

using json = nlohmann::json;
namespace {
json Create(int id, std::string type = "node", json props = json::object()) {
    return {{"op", "create"}, {"id", id}, {"elementType", type}, {"props", props}};
}
json Patch(int id, json props) { return {{"op", "patch"}, {"id", id}, {"props", props}}; }
json Children(int id, std::vector<int> children) {
    return {{"op", "setChildren"}, {"parentId", id}, {"childrenIds", children}};
}
json TableProps() { return {{"columns", json::array({{{"fieldId", "v"}, {"heading", "Value"}, {"type", "number"}}})}}; }
}

// Every structural assertion uses the production parser, dispatch, serialized
// subject and publication path. Friendship is for identity/failure/lock probes.
class XFramesTest : public ::testing::Test {
protected:
    std::unique_ptr<XFrames> xf;
    using Edges = std::initializer_list<std::pair<int, std::vector<int>>>;
    void SetUp() override { xf = std::make_unique<XFrames>("publication-test", std::nullopt); }
    json Wire(Edges edges = {}, std::initializer_list<json> operations = {}) {
        json wire = {{"schemaVersion", 2}, {"surfaceId", 0},
            {"baseRevision", xf->GetCommitState()["nativeRevision"]},
            {"rootChildren", json::array()}, {"operations", std::vector<json>(operations)}};
        for (const auto& [parent, children] : edges) {
            if (parent == 0) wire["rootChildren"] = children;
            else wire["operations"].push_back(Children(parent, children));
        }
        return wire;
    }
    xframes::CommitResult Publish(Edges edges = {}, std::initializer_list<json> operations = {}) {
        auto result = xf->ApplyCommit(Wire(edges, operations).dump());
        EXPECT_EQ(result.status, "applied") << result.ToJson();
        return result;
    }
    void Reject(json wire, const char* code, std::optional<size_t> index = std::nullopt) {
        const auto before = xf->GetDiagnosticsState(), counters = xf->GetCommitState();
        auto result = xf->ApplyCommit(wire.dump());
        ASSERT_EQ(result.status, "rejected") << result.ToJson() << wire;
        ASSERT_TRUE(result.error);
        EXPECT_EQ(result.error->code, code) << result.ToJson() << wire;
        if (index) EXPECT_EQ(result.error->operationIndex, index) << result.ToJson();
        EXPECT_FALSE(result.nativeSequence);
        EXPECT_TRUE(result.destroyedIds.empty());
        // Yoga's not-yet-laid-out dimensions are NaN; compare the actual JSON
        // observation (null for undefined dimensions), not NaN == NaN.
        EXPECT_EQ(xf->GetDiagnosticsState().dump(), before.dump());
        EXPECT_EQ(xf->GetCommitState(), counters);
    }
    json Node(const json& state, int id) {
        for (const auto& node : state["elements"]) if (node["id"] == id) return node;
        return nullptr;
    }
    Element* ElementAt(int id) { return xf->m_elements.at(id).get(); }
    auto CopyInternalSubject(int id) { return xf->m_elementInternalOpsSubject.at(id); }
    auto MapProgressCompletion(int id) {
        auto* map = static_cast<MapView*>(ElementAt(id));
        const std::lock_guard lock(map->m_async->mutex);
        ++map->m_async->prefetchInflight;
        ++map->m_async->prefetchTotal;
        return [weak = std::weak_ptr(map->m_async)] {
            MapView::CompleteTile(weak, TileKey{0, 0, 1}, true, {}, 0);
        };
    }
    void Internal(int id, const json& operation) { auto payload = operation.dump(); xf->QueueElementInternalOp(id, payload); }
    void SetCounters(uint64_t sequence, uint64_t revision) { xf->m_nativeSequence = sequence; xf->m_nativeRevision = revision; }
    bool LastRequestExpired() {
        bool expired = false;
        auto subscription = rpp::composite_disposable_wrapper::make();
        xf->m_elementOpSubject.get_observable() | rpp::ops::subscribe(subscription,
            [&](const std::weak_ptr<CommitRequest>& request) { expired = request.expired(); });
        subscription.dispose();
        return expired;
    }
    void OnButtonCreate(std::function<void()> callback) {
        auto create = xf->m_element_init_fn.at("di-button");
        xf->m_element_init_fn["di-button"] = [create, callback](const json& props, std::optional<WidgetStyle> style, XFrames* view) {
            callback();
            return create(props, style, view);
        };
    }
    void BreakCanvasBootstrap(const std::string& type) {
        xf->m_element_init_fn[type] = [type](const json& props, std::optional<WidgetStyle> style, XFrames* view) -> std::unique_ptr<Element> {
            const int id = props.at("id");
            if (type == "di-js-canvas") return std::unique_ptr<Element>(new JsCanvas(view, id, style, "function ("));
            if (type == "di-lua-canvas") return std::unique_ptr<Element>(new LuaCanvas(view, id, style, "function ("));
            return std::unique_ptr<Element>(new JanetCanvas(view, id, style, "("));
        };
    }
    // Called from a separate reader thread at a rendezvous inside real subject
    // delivery. Both locks must be owned even between individual mutations.
    void ProbeVisibilityLocks() {
        std::unique_lock hierarchy(xf->m_hierarchy_mutex, std::try_to_lock);
        std::unique_lock elements(xf->m_elements_mutex, std::try_to_lock);
        EXPECT_FALSE(hierarchy.owns_lock());
        EXPECT_FALSE(elements.owns_lock());
    }
    void InsertUnowned(int id) {
        xf->m_elements[id] = std::make_unique<Element>(xf.get(), id, false, false, false);
        xf->m_hierarchy[id] = {};
    }
    void SetUnownedRoot(int id) { xf->m_hierarchy[0] = {id}; }
    void AssertEmpty() {
        const auto state = xf->GetDiagnosticsState();
        EXPECT_EQ(state["elementCount"], 0); EXPECT_EQ(state["hierarchyCount"], 1);
        EXPECT_EQ(state["internalSubjectCount"], 0); EXPECT_EQ(state["unreachableCount"], 0);
        EXPECT_EQ(state["rootChildren"], json::array()); EXPECT_EQ(xf->GetCommitState()["managedCount"], 0);
        EXPECT_FALSE(xf->IsElementAlive(0));
    }
    size_t FormatCount() { return xf->m_floatFormatChars.size(); }
};

TEST_F(XFramesTest, PublicationSingleNodeUnmountKeepsOnlyVirtualContainer) {
    AssertEmpty();
    Publish({{0, {1}}, {1, {}}}, {Create(1)});
    EXPECT_EQ(Publish().destroyedIds, (std::vector<int>{1}));
    AssertEmpty();
}
TEST_F(XFramesTest, PublicationDeepRemovalIsOrderedAndComplete) {
    Publish({{0, {1}}, {1, {2, 4}}, {2, {3}}, {3, {}}, {4, {}}}, {Create(1), Create(2), Create(3), Create(4)});
    EXPECT_EQ(Publish().destroyedIds, (std::vector<int>{3, 2, 4, 1}));
    AssertEmpty();
}
TEST_F(XFramesTest, PublicationPartialRootRemovalPreservesSiblingIdentity) {
    Publish({{0, {1, 2}}, {1, {3}}, {2, {}}, {3, {}}}, {Create(1), Create(2), Create(3)});
    auto survivor = ElementAt(2);
    EXPECT_EQ(Publish({{0, {2}}, {2, {}}}).destroyedIds, (std::vector<int>{3, 1}));
    EXPECT_EQ(ElementAt(2), survivor);
    EXPECT_EQ(xf->GetChildren(0), (std::vector<int>{2}));
}
TEST_F(XFramesTest, PublicationReplacementDestroysOnlyOldLifetimes) {
    Publish({{0, {1, 2}}, {1, {}}, {2, {}}}, {Create(1), Create(2)});
    EXPECT_EQ(Publish({{0, {3, 4}}, {3, {}}, {4, {}}}, {Create(3), Create(4)}).destroyedIds, (std::vector<int>{1, 2}));
    EXPECT_EQ(xf->GetDiagnosticsState()["elementCount"], 2);
}
TEST_F(XFramesTest, PublicationReorderAndInsertionKeepHierarchyAndYogaConsistent) {
    Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {}}}, {Create(1), Create(2), Create(3)});
    auto survivor = ElementAt(3);
    EXPECT_TRUE(Publish({{0, {1}}, {1, {3, 4, 2}}, {3, {}}, {4, {}}, {2, {}}}, {Create(4)}).destroyedIds.empty());
    const auto state = xf->GetDiagnosticsState();
    EXPECT_EQ(ElementAt(3), survivor);
    EXPECT_EQ(Node(state, 1)["children"], json::array({3, 4, 2}));
    EXPECT_EQ(Node(state, 1)["yogaChildren"], json::array({3, 4, 2}));
    EXPECT_EQ(Node(state, 4)["yogaParent"], 1);
}
TEST_F(XFramesTest, PublicationIdenticalAndEmptyTreesAdvanceExactlyOnce) {
    EXPECT_EQ(Publish().nativeRevision, 1);
    EXPECT_EQ(Publish({{0, {1}}, {1, {}}}, {Create(1)}).nativeRevision, 2);
    auto element = ElementAt(1);
    auto unchanged = Publish({{0, {1}}, {1, {}}}, {Patch(1, json::object())});
    EXPECT_EQ(unchanged.nativeRevision, 3); EXPECT_EQ(unchanged.nativeSequence, 3);
    EXPECT_EQ(ElementAt(1), element); EXPECT_TRUE(unchanged.destroyedIds.empty());
    EXPECT_EQ(Publish().nativeRevision, 4); EXPECT_EQ(Publish().nativeRevision, 5);
    AssertEmpty();
}
TEST_F(XFramesTest, PublicationDoesNotDestroyGlobalFloatFormats) {
    ASSERT_EQ(FormatCount(), 10);
    Publish({{0, {3}}, {3, {}}}, {Create(3)});
    Publish(); EXPECT_EQ(FormatCount(), 10);
}
TEST_F(XFramesTest, PublicationRejectsOldWireVersionAndRemovedOperations) {
    Reject({{"schemaVersion", 1}, {"surfaceId", 0}, {"operations", json::array()}}, "unsupported_version");
    Reject(Wire({}, {{{"op", "appendChild"}, {"parentId", 1}, {"childId", 2}}}), "unsupported_operation", 0);
}
TEST_F(XFramesTest, PublicationEnvelopeValidationIsStrictAndNonMutating) {
    for (const auto& [field, value, code] : std::vector<std::tuple<std::string, json, const char*>>{
        {"surfaceId", 1, "unsupported_surface"}, {"surfaceId", "0", "unsupported_surface"},
        {"schemaVersion", 99, "unsupported_version"}, {"operations", json::object(), "invalid_field"},
        {"rootChildren", json::object(), "invalid_field"}, {"sequence", 1, "unknown_field"},
        {"correlationId", std::string(129, 'a'), "invalid_field"}}) {
        auto wire = Wire(); wire[field] = value; Reject(wire, code);
    }
    for (const auto& field : {"schemaVersion", "surfaceId", "baseRevision", "rootChildren", "operations"}) {
        auto wire = Wire(); wire.erase(field); Reject(wire, "missing_field");
    }
    const auto invalid = xf->ApplyCommit("{"); ASSERT_TRUE(invalid.error); EXPECT_EQ(invalid.error->code, "invalid_json");
    AssertEmpty(); EXPECT_EQ(Publish().nativeRevision, 1);
}
TEST_F(XFramesTest, PublicationIdsAndRevisionStringsHaveLosslessRanges) {
    for (auto id : json::array({0, -1, 1.5, 2147483648LL, 9007199254740993LL, "2", nullptr})) {
        auto op = Create(2); op["id"] = id; Reject(Wire({}, {op}), "invalid_id", 0);
    }
    for (auto revision : json::array({0, "", "00", "01", "-1", "+1", "1.0", "18446744073709551616", nullptr})) {
        auto wire = Wire(); wire["baseRevision"] = revision; Reject(wire, "invalid_field");
    }
    Publish({{0, {2147483647}}, {2147483647, {}}}, {Create(2147483647)});
}
TEST_F(XFramesTest, PublicationRejectsStaleRevisionAndAllowsExplicitRecovery) {
    auto stale = Wire({{0, {1}}, {1, {}}}, {Create(1)});
    Publish(); Reject(stale, "stale_revision");
    stale["baseRevision"] = "1";
    EXPECT_EQ(xf->ApplyCommit(stale.dump()).status, "applied");
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], "2");
}
TEST_F(XFramesTest, PublicationValidatesEntireCandidateBeforeApplyingAPrefix) {
    Publish({{0, {1}}, {1, {}}}, {Create(1)});
    const std::vector<std::pair<json, const char*>> invalid = {
        {Create(1), "duplicate_id"}, {Create(3, "unknown"), "invalid_element_type"},
        {Patch(999, json::object()), "missing_target"}, {Patch(1, {{"id", "public"}}), "immutable_identity"},
        {Patch(1, {{"type", "node"}}), "immutable_identity"}, {Patch(1, {{"root", true}}), "immutable_identity"},
        {Patch(1, {{"style", {{"border", {{"thickness", "bad"}}}}}}), "invalid_props"},
        {Create(3, "di-table", {{"columns", json::array({{{"heading", "missing fieldId"}}})}}), "invalid_props"}
    };
    for (const auto& [operation, code] : invalid)
        Reject(Wire({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2), Patch(1, {{"style", {{"width", 123}}}}), operation}), code, 2);
}
TEST_F(XFramesTest, PublicationRequiresOneAssignmentForEveryFinalNode) {
    Reject(Wire({{0, {1}}}, {Create(1)}), "missing_children");
    Reject(Wire({{0, {1}}, {1, {}}}, {Create(1), Children(1, {})}), "duplicate_assignment", 2);
    Reject(Wire({{0, {1}}, {1, {99}}}, {Create(1)}), "missing_target", 1);
    Reject(Wire({{0, {1}}, {1, {2, 2}}, {2, {}}}, {Create(1), Create(2)}), "duplicate_child", 2);
    Reject(Wire({{0, {1, 1}}, {1, {}}}, {Create(1)}), "duplicate_child");
}
TEST_F(XFramesTest, PublicationRejectsUnreachableCreatesPatchesAndDeclarations) {
    Reject(Wire({}, {Create(1)}), "unreachable_operation", 0);
    Publish({{0, {1}}, {1, {}}}, {Create(1)});
    Reject(Wire({}, {Patch(1, json::object())}), "unreachable_operation", 0);
    Reject(Wire({{1, {}}}), "unreachable_operation", 0);
}
TEST_F(XFramesTest, PublicationValidatesFinalOwnershipCyclesAndRootRestrictions) {
    Reject(Wire({{0, {1, 2}}, {1, {3}}, {2, {3}}, {3, {}}}, {Create(1), Create(2), Create(3)}), "multiple_parents");
    Reject(Wire({{1, {2}}, {2, {1}}}, {Create(1), Create(2)}), "cycle");
    Reject(Wire({{0, {1}}, {1, {1}}}, {Create(1)}), "cycle");
    Reject(Wire({{0, {1}}, {1, {2}}, {2, {}}}, {Create(1), Create(2, "node", {{"root", true}})}), "invalid_relationship");
    Reject(Wire({{0, {1}}, {1, {2}}, {2, {}}}, {Create(1, "unformatted-text", {{"text", "leaf"}}), Create(2)}), "invalid_relationship");
}
TEST_F(XFramesTest, PublicationAllowsForwardReferencesAndOrderedPatches) {
    auto wire = Wire({{0, {1}}, {1, {2}}, {2, {}}}, {Patch(2, {{"style", {{"width", 21}}}}), Create(2), Create(1), Patch(2, {{"style", {{"width", 42}}}})});
    std::reverse(wire["operations"].begin() + 4, wire["operations"].end());
    auto result = xf->ApplyCommit(wire.dump()); ASSERT_EQ(result.status, "applied") << result.ToJson();
    EXPECT_EQ(YGNodeStyleGetWidth(ElementAt(2)->m_layoutNode->m_node).value, 42);
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["yogaParent"], 1);
}
TEST_F(XFramesTest, PublicationCannotDestroyAndRecreateAnAcknowledgmentIdentity) {
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(1), Create(2)});
    Reject(Wire({{0, {1, 2}}, {1, {}}, {2, {}}}, {Create(2)}), "duplicate_id", 0);
    EXPECT_EQ(Publish({{0, {1}}, {1, {}}}).destroyedIds, (std::vector<int>{2}));
    Reject(Wire({{0, {1}}, {1, {2}}, {2, {}}}), "missing_target");
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2)}); // explicitly new lifetime, later publication
}
TEST_F(XFramesTest, PublicationNeitherAdoptsNorSweepsUnownedObjects) {
    InsertUnowned(99); auto unowned = ElementAt(99);
    Publish({{0, {1}}, {1, {}}}, {Create(1)});
    Reject(Wire({{0, {1, 99}}, {1, {}}, {99, {}}}), "ownership_conflict");
    Publish(); EXPECT_EQ(ElementAt(99), unowned); EXPECT_EQ(xf->GetDiagnosticsState()["elementCount"], 1);
    SetUnownedRoot(99);
    Reject(Wire(), "ownership_conflict");
}
TEST_F(XFramesTest, PublicationCountersSurviveDiagnosticTogglesAndSubjectSetup) {
    auto wire = Wire(); wire["correlationId"] = "opaque";
    EXPECT_EQ(xf->ApplyCommit(wire.dump()).ToJson()["correlationId"], "opaque");
    xf->SetDiagnosticsEnabled(true); Publish(); xf->SetDiagnosticsEnabled(false); xf->SetUpSubjects(); Publish();
    EXPECT_EQ(xf->GetCommitState()["nativeSequence"], "3"); EXPECT_EQ(xf->GetCommitState()["nativeRevision"], "3");
    EXPECT_TRUE(LastRequestExpired());
}
TEST_F(XFramesTest, PublicationCountersRemainLosslessAndOverflowRejects) {
    SetCounters(9007199254740992ULL, 9007199254740992ULL);
    EXPECT_EQ(Publish().ToJson()["nativeRevision"], "9007199254740993");
    SetCounters(UINT64_MAX, 12); Reject(Wire(), "counter_overflow");
    SetCounters(12, UINT64_MAX); Reject(Wire(), "counter_overflow");
}
TEST_F(XFramesTest, PublicationCompetingWritersCannotOverwriteAStaleTree) {
    Publish({{0, {1}}, {1, {}}}, {Create(1)});
    const auto wire = Wire({{0, {1}}, {1, {}}}, {Patch(1, {{"style", {{"width", 30}}}})}).dump();
    std::barrier rendezvous(3);
    xframes::CommitResult first, second;
    std::thread a([&] { rendezvous.arrive_and_wait(); first = xf->ApplyCommit(wire); });
    std::thread b([&] { rendezvous.arrive_and_wait(); second = xf->ApplyCommit(wire); });
    rendezvous.arrive_and_wait(); a.join(); b.join();
    EXPECT_EQ((first.status == "applied") + (second.status == "applied"), 1);
    const auto& rejected = first.status == "rejected" ? first : second;
    ASSERT_TRUE(rejected.error); EXPECT_EQ(rejected.error->code, "stale_revision");
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], "2");
}

// The same subject path plus actual ImGui/ImPlot frame construction; no window
// or graphics driver is involved. Real Node/Wasm suites establish GPU coverage.
class XFramesQueueTest : public XFramesTest {
protected:
    std::unique_ptr<ImPlotRenderer> renderer;
    void SetUp() override {
        XFramesTest::SetUp();
        std::string fonts = "{}";
        renderer = std::make_unique<ImPlotRenderer>(xf.get(), "publication-test", "publication-test", fonts, std::nullopt);
        auto& io = ImGui::GetIO(); io.DisplaySize = ImVec2(900, 700); io.DeltaTime = 1.0f / 60;
        io.FontDefault = io.Fonts->AddFontDefault();
        unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        xf->m_onInit = [] {}; xf->m_onTableSort = [](int, int, int) {};
        xf->m_onTableFilter = [](int, int, const std::string&) {}; xf->m_onTableRowClick = [](int, int) {};
        xf->Init(renderer.get());
        Publish({{0, {1}}, {1, {}}}, {Create(1, "node", {{"root", true}, {"style", {{"width", 850}, {"height", 650}}}})});
    }
    void TearDown() override {
        xf.reset();
        if (ImPlot::GetCurrentContext()) ImPlot::DestroyContext();
        if (renderer->m_imGuiCtx) ImGui::DestroyContext(renderer->m_imGuiCtx);
        renderer.reset();
    }
    void Frame() { xf->Render(900, 700); xf->CompleteDiagnosticsFrame(); }
};

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

TEST_F(XFramesQueueTest, PublicationPreservesImperativeDataAndDestroysSubjects) {
    xf->SetDiagnosticsEnabled(true);
    Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {}}},
        {Create(2, "plot-bar", {{"series", json::array({{{"label", "A"}}, {{"label", "B"}}})}}), Create(3, "di-table", TableProps())});
    Internal(2, {{"op", "appendSeriesData"}, {"seriesIndex", 1}, {"x", 42}, {"y", 100}});
    Internal(3, {{"op", "setData"}, {"data", json::array({{{"v", 42}}})}});
    auto state = xf->GetDiagnosticsState();
    EXPECT_EQ(state["internalSubjectCount"], 2);
    EXPECT_EQ(Node(state, 2)["state"]["series"][1]["lastX"], 42);
    EXPECT_EQ(Node(state, 3)["state"]["rowCount"], 1);
    EXPECT_GT(Node(state, 2)["lastInternalOpMs"].get<double>(), 0);
    EXPECT_EQ(Publish({{0, {1}}, {1, {}}}).destroyedIds, (std::vector<int>{2, 3}));
    Internal(2, {{"op", "appendData"}, {"x", 43}, {"y", 0}});
    EXPECT_EQ(xf->GetDiagnosticsState()["internalSubjectCount"], 0);
    EXPECT_FALSE(xf->IsElementAlive(2));
}
TEST_F(XFramesQueueTest, XF_LIFE_005_SameIdMovePreservesExactWidgetsSubjectsAndPopulatedData) {
    Publish({{0, {1}}, {1, {2, 3}}, {2, {4, 5}}, {3, {}}, {4, {}}, {5, {}}},
        {Create(2), Create(3), Create(4, "plot-bar"), Create(5, "di-table", TableProps())});
    Internal(4, {{"op", "appendData"}, {"x", 42}, {"y", 7}});
    Internal(5, {{"op", "setData"}, {"data", json::array({{{"v", 19}}})}});
    auto plot = ElementAt(4), table = ElementAt(5); auto yoga = plot->m_layoutNode->m_node;
    auto subject = CopyInternalSubject(4); auto tableSubject = CopyInternalSubject(5);
    const auto before = xf->GetDiagnosticsState();
    // Receiving assignment precedes detachment in the envelope.
    const auto result = Publish({{0, {1}}, {1, {2, 3}}, {3, {5, 4}}, {2, {}}, {4, {}}, {5, {}}});
    EXPECT_TRUE(result.destroyedIds.empty()); EXPECT_EQ(ElementAt(4), plot); EXPECT_EQ(ElementAt(5), table);
    EXPECT_EQ(plot->m_layoutNode->m_node, yoga);
    EXPECT_EQ(CopyInternalSubject(4).get_disposable(), subject.get_disposable());
    EXPECT_EQ(CopyInternalSubject(5).get_disposable(), tableSubject.get_disposable());
    const auto after = xf->GetDiagnosticsState();
    EXPECT_EQ(Node(after, 4)["state"], Node(before, 4)["state"]); EXPECT_EQ(Node(after, 5)["state"], Node(before, 5)["state"]);
    EXPECT_EQ(Node(after, 2)["yogaChildren"], json::array()); EXPECT_EQ(Node(after, 3)["yogaChildren"], json::array({5, 4}));
    EXPECT_EQ(Node(after, 4)["yogaParent"], 3); EXPECT_EQ(after["internalSubjectCount"], 2);
    Frame();
}
TEST_F(XFramesQueueTest, PublicationMovesSurvivingDescendantsOutOfRemovedAncestors) {
    Publish({{0, {1}}, {1, {2, 3}}, {2, {4}}, {3, {}}, {4, {5}}, {5, {}}},
        {Create(2), Create(3), Create(4), Create(5, "plot-bar")});
    Internal(5, {{"op", "appendData"}, {"x", 21}, {"y", 8}}); auto plot = ElementAt(5);
    EXPECT_EQ(Publish({{0, {1}}, {1, {3}}, {3, {5}}, {5, {}}}).destroyedIds, (std::vector<int>{4, 2}));
    EXPECT_EQ(ElementAt(5), plot); EXPECT_EQ(Node(xf->GetDiagnosticsState(), 5)["yogaParent"], 3);
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 5)["state"]["series"][0]["lastX"], 21); Frame();
    EXPECT_EQ(Publish().destroyedIds, (std::vector<int>{5, 3, 1})); AssertEmpty();
}
TEST_F(XFramesQueueTest, PublicationDelayedSubjectCannotTargetAReusedNativeId) {
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "plot-bar")});
    auto old = CopyInternalSubject(2);
    Internal(2, {{"op", "appendData"}, {"x", 1}, {"y", 2}});
    EXPECT_EQ(Publish({{0, {1}}, {1, {}}}).destroyedIds, (std::vector<int>{2}));
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "plot-bar")});
    Internal(2, {{"op", "appendData"}, {"x", 10}, {"y", 20}});
    old.get_observer().on_next({{"op", "appendData"}, {"x", 99}, {"y", 99}});
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["state"]["series"][0]["lastX"], 10);
}
TEST_F(XFramesQueueTest, PublicationGuardedNullRemovalPreservesWidgetData) {
    Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {}}}, {Create(2, "plot-bar"), Create(3, "di-table", TableProps())});
    Internal(2, {{"op", "appendData"}, {"x", 12}, {"y", 21}}); Internal(3, {{"op", "setData"}, {"data", json::array({{{"v", 19}}})}});
    const auto before = xf->GetDiagnosticsState();
    Publish({{0, {1}}, {1, {2, 3, 4, 5, 6}}, {2, {}}, {3, {}}, {4, {}}, {5, {}}, {6, {}}}, {
        Patch(2, {{"series", nullptr}, {"bullColor", nullptr}}), Patch(3, {{"columns", nullptr}, {"contextMenuItems", nullptr}, {"clipRows", nullptr}}),
        Create(4, "multi-slider", {{"numValues", nullptr}, {"decimalDigits", nullptr}, {"defaultValues", nullptr}}),
        Create(5, "color-indicator", {{"color", nullptr}}), Create(6, "di-window"),
        Patch(6, {{"title", nullptr}, {"width", nullptr}, {"height", nullptr}}),
        Patch(4, {{"style", {{"font", nullptr}, {"colors", nullptr}, {"vars", nullptr}, {"roundCorners", nullptr}}}})});
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["state"], Node(before, 2)["state"]);
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 3)["state"], Node(before, 3)["state"]);
    EXPECT_EQ(Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {}}}).destroyedIds, (std::vector<int>{4, 5, 6}));
}
TEST_F(XFramesQueueTest, PublicationThousandCyclesReturnToBaselineIncludingMovesAndKeyedReplacement) {
    Publish(); const auto baseline = xf->GetDiagnosticsState();
    for (int cycle = 0; cycle < 1000; ++cycle) {
        Publish({{0, {1}}, {1, {2, 3}}, {2, {4, 5}}, {3, {}}, {4, {}}, {5, {}}},
            {Create(1, "node", {{"root", true}}), Create(2), Create(3), Create(4, "plot-bar"), Create(5, "di-table", TableProps())});
        Internal(4, {{"op", "appendData"}, {"x", cycle}, {"y", 7}});
        Internal(5, {{"op", "setData"}, {"data", json::array({{{"v", cycle}}})}});
        auto plot = ElementAt(4);
        Publish({{0, {1}}, {1, {2, 3}}, {3, {4, 5}}, {2, {}}, {4, {}}, {5, {}}});
        ASSERT_EQ(ElementAt(4), plot); ASSERT_EQ(Node(xf->GetDiagnosticsState(), 4)["state"]["series"][0]["lastX"], cycle);
        if (cycle % 2 == 0) {
            EXPECT_EQ(Publish({{0, {1}}, {1, {}}}).destroyedIds, (std::vector<int>{2, 4, 5, 3}));
            Publish();
        } else {
            EXPECT_EQ(Publish({{0, {1}}, {1, {3}}, {3, {6, 5}}, {6, {}}, {5, {}}}, {Create(6, "plot-bar")}).destroyedIds, (std::vector<int>{2, 4}));
            Internal(6, {{"op", "appendData"}, {"x", cycle}, {"y", 9}});
            EXPECT_EQ(Publish().destroyedIds, (std::vector<int>{6, 5, 3, 1}));
        }
        ASSERT_EQ(xf->GetDiagnosticsState(), baseline) << cycle;
        ASSERT_TRUE(LastRequestExpired());
    }
    AssertEmpty();
}
TEST_F(XFramesQueueTest, PublicationFailureQuarantinesBeforeReadersOrDrawingCanResume) {
    OnButtonCreate([] { throw std::runtime_error("injected constructor failure"); });
    auto wire = Wire({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {}}}, {Create(2), Create(3, "di-button", {{"label", "fail"}})});
    const auto result = xf->ApplyCommit(wire.dump());
    EXPECT_EQ(result.status, "failed"); EXPECT_EQ(result.nativeSequence, 2); EXPECT_EQ(result.nativeRevision, 1);
    ASSERT_TRUE(result.error); EXPECT_EQ(result.error->code, "application_error"); EXPECT_EQ(result.error->operationIndex, 1);
    EXPECT_EQ(xf->GetCommitState()["surfaceStatus"], "quarantined");
    EXPECT_FALSE(xf->IsElementAlive(1)); EXPECT_FALSE(xf->IsElementAlive(2));
    EXPECT_THROW(xf->GetChildren(0), xframes::CommitError);
    EXPECT_THROW(xf->GetDiagnosticsState(), xframes::CommitError);
    const auto rejected = xf->ApplyCommit(Wire().dump()); ASSERT_TRUE(rejected.error); EXPECT_EQ(rejected.error->code, "surface_quarantined");
    xf->SetDiagnosticsEnabled(true); Frame(); EXPECT_EQ(xf->GetDiagnosticsFrame()["surfaceStatus"], "quarantined");
    EXPECT_EQ(xf->GetCommitState()["nativeSequence"], "2"); EXPECT_TRUE(LastRequestExpired());
}
TEST_F(XFramesQueueTest, CanvasBootstrapFailureQuarantinesWithoutCallingApplicationScriptHandlers) {
    for (const auto& type : {"di-js-canvas", "di-lua-canvas", "di-janet-canvas"}) {
        SCOPED_TRACE(type);
        xf = std::make_unique<XFrames>("bootstrap-failure", std::nullopt);
        static int callbacks = 0;
        callbacks = 0;
        xf->m_onScriptError = [](int, const std::string&) { ++callbacks; };
        BreakCanvasBootstrap(type);
        const auto result = xf->ApplyCommit(Wire({{0, {1}}, {1, {2}}, {2, {}}},
            {Create(1, "node", {{"root", true}}), Create(2, type)}).dump());
        EXPECT_EQ(result.status, "failed"); ASSERT_TRUE(result.error);
        EXPECT_EQ(result.error->code, "application_error"); EXPECT_EQ(result.error->operationIndex, 1);
        EXPECT_EQ(result.nativeRevision, 0); EXPECT_EQ(result.nativeSequence, 1);
        EXPECT_EQ(callbacks, 0); EXPECT_FALSE(xf->IsElementAlive(1)); EXPECT_FALSE(xf->IsElementAlive(2));
        EXPECT_EQ(xf->GetCommitState()["surfaceStatus"], "quarantined");
        EXPECT_TRUE(LastRequestExpired());
        // Failed constructors must release their engine and GC ownership. A new
        // runtime can still create and delete the same real widget normally.
        xf = std::make_unique<XFrames>("after-bootstrap-failure", std::nullopt);
        Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(1, "node", {{"root", true}}), Create(2, type)});
        EXPECT_EQ(Publish().destroyedIds, (std::vector<int>{2, 1})); AssertEmpty();
    }
}
TEST_F(XFramesQueueTest, PublicationVisibilityLocksExcludeCoordinatedReaderAndRendererThroughRealSubject) {
    Publish({{0, {1}}, {1, {2, 4}}, {2, {}}, {4, {}}}, {Create(2), Create(4, "plot-bar")});
    const auto next = Wire({{0, {1}}, {1, {3, 5}}, {3, {}}, {5, {}}},
        {Create(3), Create(5, "di-button", {{"label", "new"}}), Patch(1, {{"style", {{"width", 400}}}})}).dump();
    std::latch inside(1), probed(2), release(1);
    OnButtonCreate([&] { inside.count_down(); release.wait(); });
    auto writer = std::async(std::launch::async, [&] { return xf->ApplyCommit(next); });
    inside.wait();
    auto reader = std::async(std::launch::async, [&] {
        ProbeVisibilityLocks(); probed.count_down(); return xf->GetDiagnosticsState();
    });
    auto render = std::async(std::launch::async, [&] {
        ProbeVisibilityLocks(); probed.count_down(); xf->Render(900, 700);
    });
    probed.wait();
    // These are nonblocking observations at a deterministic rendezvous, not
    // sleep-based attempts to miss a race. Neither operation can pass tree locks.
    EXPECT_EQ(reader.wait_for(std::chrono::seconds(0)), std::future_status::timeout);
    EXPECT_EQ(render.wait_for(std::chrono::seconds(0)), std::future_status::timeout);
    release.count_down();
    const auto result = writer.get(); EXPECT_EQ(result.status, "applied");
    EXPECT_EQ(result.destroyedIds, (std::vector<int>{2, 4}));
    const auto state = reader.get(); render.get();
    EXPECT_EQ(state["elementCount"], 3); EXPECT_EQ(state["internalSubjectCount"], 0);
    EXPECT_EQ(Node(state, 1)["children"], json::array({3, 5})); EXPECT_EQ(Node(state, 1)["yogaChildren"], json::array({3, 5}));
    EXPECT_TRUE(Node(state, 2).is_null()); EXPECT_TRUE(Node(state, 4).is_null());
    EXPECT_EQ(YGNodeStyleGetWidth(ElementAt(1)->m_layoutNode->m_node).value, 400);
}

TEST_F(XFramesQueueTest, FrameDiagnosticsAreOptInAndPublishTheConstructedState) {
    Frame();
    EXPECT_FALSE(xf->GetDiagnosticsFrame().contains("frameId"));
    xf->SetDiagnosticsEnabled(true);
    xf->Render(900, 700);
    // Mutations after construction must not be misattributed to that frame.
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2)});
    EXPECT_FALSE(xf->GetDiagnosticsFrame().contains("frameId"));
    xf->CompleteDiagnosticsFrame();
    auto frame = xf->GetDiagnosticsFrame();
    EXPECT_EQ(frame["frameId"], "2");
    EXPECT_EQ(frame["elementCount"], 1);
    EXPECT_GE(frame["submittedAtMs"].get<double>(), frame["constructedAtMs"].get<double>());
    Frame();
    frame = xf->GetDiagnosticsFrame();
    EXPECT_EQ(frame["frameId"], "3");
    EXPECT_EQ(frame["elementCount"], 2);
    xf->SetDiagnosticsEnabled(false);
    Frame();
    EXPECT_EQ(xf->GetDiagnosticsFrame()["frameId"], "3");
    EXPECT_EQ(xf->GetDiagnosticsFrame()["enabled"], false);
    // An unsubmitted frame must not be published after disabling diagnostics.
    xf->SetDiagnosticsEnabled(true);
    xf->Render(900, 700);
    xf->SetDiagnosticsEnabled(false);
    Frame();
    EXPECT_EQ(xf->GetDiagnosticsFrame()["frameId"], "3");
}

TEST_F(XFramesTest, PublicationInvalidatesOnceWhileRejectionsAndQueriesAreObservational) {
    const auto initial = xf->m_frameScheduler.GetState();
    EXPECT_EQ(initial["generation"], "1");
    Publish({{0, {1}}, {1, {}}}, {Create(1)});
    auto scheduled = xf->m_frameScheduler.GetState();
    EXPECT_EQ(scheduled["generation"], "2");
    EXPECT_EQ(scheduled["reasons"]["publication"]["invalidations"], "1");
    auto invalid = Wire(); invalid["schemaVersion"] = 1;
    Reject(invalid, "unsupported_version");
    for (int i = 0; i < 100; ++i) {
        xf->GetDiagnosticsFrame(); xf->GetDiagnosticsState(); xf->GetCommitState();
        xf->IsElementAlive(1); xf->GetChildren(0);
    }
    EXPECT_EQ(xf->m_frameScheduler.GetState(), scheduled);
    Publish({{0, {1}}, {1, {}}}); // Identical accepted publication still invalidates.
    Publish();
    Publish(); // Accepted empty publication also advances exactly once.
    EXPECT_EQ(xf->m_frameScheduler.GetState()["generation"], "5");
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], "4");
}

TEST_F(XFramesQueueTest, SubjectPublicationAfterDrawCaptureStaysPendingUntilItsOwnCoveringFrame) {
    xf->SetDiagnosticsEnabled(true);
    const auto revision = xf->GetCommitState()["nativeRevision"];
    const auto generation = xf->m_frameScheduler.GetState()["generation"];
    std::latch captured(1), published(1);
    auto next = Wire({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2)});
    std::thread publisher([&] {
        captured.wait();
        EXPECT_EQ(xf->ApplyCommit(next.dump()).status, "applied");
        published.count_down();
    });
    const bool constructed = xf->Render(900, 700);
    captured.count_down();
    published.wait();
    publisher.join();
    ASSERT_TRUE(constructed);
    xf->CompleteDiagnosticsFrame();
    const auto earlier = xf->GetDiagnosticsFrame();
    EXPECT_EQ(earlier["nativeRevision"], revision);
    EXPECT_EQ(earlier["coveredGeneration"], generation);
    EXPECT_EQ(earlier["elementCount"], 1);
    EXPECT_TRUE(earlier["scheduler"]["dirty"]);
    ASSERT_TRUE(xf->m_frameScheduler.TakeOpportunity().render);
    Frame();
    const auto later = xf->GetDiagnosticsFrame();
    EXPECT_EQ(later["nativeRevision"], xf->GetCommitState()["nativeRevision"]);
    EXPECT_EQ(later["coveredGeneration"], later["scheduler"]["generation"]);
    EXPECT_EQ(later["elementCount"], 2);
    EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render);
}

TEST_F(XFramesQueueTest, ImperativeCoverageAdvancesWithoutStructuralRevisionOrDiagnostics) {
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "plot-bar")});
    Frame();
    const auto revision = xf->GetCommitState()["nativeRevision"];
    auto before = xf->m_frameScheduler.GetState();
    EXPECT_FALSE(before["dirty"]);
    ASSERT_TRUE(xf->Render(900, 700));
    Internal(2, {{"op", "appendData"}, {"x", 42}, {"y", 21}});
    const auto after = xf->m_frameScheduler.GetState();
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], revision);
    EXPECT_NE(after["generation"], before["generation"]);
    xf->CompleteDiagnosticsFrame();
    EXPECT_EQ(xf->m_frameScheduler.GetState()["coveredGeneration"], before["generation"]);
    EXPECT_TRUE(xf->m_frameScheduler.TakeOpportunity().render);
    Frame();
    EXPECT_EQ(xf->m_frameScheduler.GetState()["coveredGeneration"], after["generation"]);
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["state"]["series"][0]["lastX"], 42);
    EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render);
    EXPECT_FALSE(xf->GetDiagnosticsFrame()["enabled"]);
    EXPECT_FALSE(xf->GetDiagnosticsFrame().contains("frameId")); // Expensive samples stayed off.
}

TEST_F(XFramesQueueTest, CanvasAnimationStaticRedrawAndSameIdMoveUseLifetimeOwnedActivity) {
    for (const auto* type : {"di-js-canvas", "di-lua-canvas", "di-janet-canvas"}) {
        Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {}}}, {Create(2, type), Create(3)});
        Internal(2, {{"op", "setScript"}, {"script", ""}});
        Frame();
        const auto* identity = ElementAt(2);
        EXPECT_EQ(xf->m_frameScheduler.GetState()["reasons"]["canvas"]["activeOwners"], 1) << type;
        EXPECT_TRUE(xf->m_frameScheduler.TakeOpportunity().render);
        Internal(2, {{"op", "setContinuous"}, {"continuous", false}});
        Frame();
        EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render) << type;
        const auto revision = xf->GetCommitState()["nativeRevision"];
        Internal(2, {{"op", "redraw"}});
        EXPECT_TRUE(xf->m_frameScheduler.TakeOpportunity().render);
        Frame();
        EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render);
        EXPECT_EQ(xf->GetCommitState()["nativeRevision"], revision);
        Publish({{0, {1}}, {1, {3}}, {3, {2}}, {2, {}}});
        Frame();
        EXPECT_EQ(ElementAt(2), identity);
        EXPECT_EQ(xf->m_frameScheduler.GetState()["ownerCount"], 1);
        EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render); // Static policy survived move.
        Internal(2, {{"op", "setContinuous"}, {"continuous", true}});
        Frame();
        EXPECT_TRUE(xf->m_frameScheduler.TakeOpportunity().render);
        Publish({{0, {1}}, {1, {}}});
        Frame();
        EXPECT_EQ(xf->m_frameScheduler.GetState()["ownerCount"], 0);
        EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render);
    }
}

TEST_F(XFramesQueueTest, AllDesktopImageJobsDrainIncludingFailuresAndClippedElements) {
    Publish({{0, {1}}, {1, {2, 3, 4}}, {2, {}}, {3, {}}, {4, {}}}, {
        Create(2, "di-image", {{"url", "missing-first.png"}, {"style", {{"width", 80}, {"height", 60}}}}),
        Create(3, "di-image", {{"url", "missing-second.png"}, {"style", {{"width", 80}, {"height", 60}}}}),
        Create(4, "di-image", {{"url", "missing-clipped.png"}, {"style", {{"display", "none"}}}})
    });
    auto state = xf->GetDiagnosticsState();
    for (int id : {2, 3, 4}) EXPECT_EQ(Node(state, id)["resources"]["queuedLoads"], 1);
    Frame();
    state = xf->GetDiagnosticsState();
    EXPECT_EQ(ImGui::GetCurrentContext()->ErrorCountCurrentFrame, 0);
    for (int id : {2, 3, 4}) {
        EXPECT_EQ(Node(state, id)["resources"]["queuedLoads"], 0);
        EXPECT_TRUE(Node(state, id)["resources"]["lastLoadFailed"]);
    }
    EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render);
    Publish({{0, {1}}, {1, {}}}); Frame();
    EXPECT_EQ(xf->m_frameScheduler.GetState()["ownerCount"], 0);
}

TEST_F(XFramesQueueTest, CanvasFileScriptsAndTextureCancellationAreSharedAcrossAllEngines) {
    const auto script = (std::filesystem::path(__FILE__).parent_path() / "fixtures/canvas-script.txt").string();
    for (const auto* type : {"di-js-canvas", "di-lua-canvas", "di-janet-canvas"}) {
        Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, type)});
        Internal(2, {{"op", "setScriptFile"}, {"path", script}});
        EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["resources"]["queuedLoads"], 1) << type;
        Frame();
        EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["resources"]["queuedLoads"], 0);
        EXPECT_EQ(xf->m_frameScheduler.GetState()["reasons"]["canvas"]["activeOwners"], 1);
        Internal(2, {{"op", "clear"}});
        Internal(2, {{"op", "loadTexture"}, {"textureId", "pending"}, {"source", __FILE__}});
        EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["resources"]["queuedLoads"], 1);
        Internal(2, {{"op", "unloadTexture"}, {"textureId", "pending"}});
        EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["resources"]["queuedLoads"], 0);
        Frame();
        EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render);
        Publish({{0, {1}}, {1, {}}}); Frame();
        EXPECT_EQ(xf->m_frameScheduler.GetState()["ownerCount"], 0);
    }
}

TEST_F(XFramesQueueTest, PendingImageMovesRetainOwnershipAndRemovalCannotFeedAReusedId) {
    Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {}}},
        {Create(2, "di-image", {{"url", "missing-before-move.png"}}), Create(3)});
    const auto* image = ElementAt(2);
    Publish({{0, {1}}, {1, {3}}, {3, {2}}, {2, {}}});
    EXPECT_EQ(ElementAt(2), image);
    EXPECT_EQ(xf->m_frameScheduler.GetState()["ownerCount"], 1);
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["resources"]["queuedLoads"], 1);
    Publish({{0, {1}}, {1, {}}});
    EXPECT_EQ(xf->m_frameScheduler.GetState()["ownerCount"], 0);
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "di-image", {{"url", "missing-new-lifetime.png"}})});
    Internal(2, {{"op", "reloadImage"}});
    Frame();
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["resources"]["queuedLoads"], 0);
    EXPECT_EQ(xf->m_frameScheduler.GetState()["ownerCount"], 1);
    EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render);
}

TEST_F(XFramesQueueTest, DiagnosticTogglesPreserveCoverageAndOnlyEnablingRequestsWork) {
    Frame();
    auto before = xf->m_frameScheduler.GetState();
    xf->SetDiagnosticsEnabled(false);
    xf->SetUpSubjects();
    EXPECT_EQ(xf->m_frameScheduler.GetState(), before);
    xf->SetDiagnosticsEnabled(true);
    auto enabled = xf->m_frameScheduler.GetState();
    EXPECT_NE(enabled["generation"], before["generation"]);
    EXPECT_EQ(enabled["coveredGeneration"], before["coveredGeneration"]);
    xf->SetDiagnosticsEnabled(true);
    EXPECT_EQ(xf->m_frameScheduler.GetState(), enabled);
    Frame();
    auto frame = xf->GetDiagnosticsFrame();
    EXPECT_EQ(frame["coveredGeneration"], enabled["generation"]);
    EXPECT_EQ(frame["nativeRevision"], xf->GetCommitState()["nativeRevision"]);
    xf->SetDiagnosticsEnabled(false);
    before = xf->m_frameScheduler.GetState();
    EXPECT_EQ(xf->GetDiagnosticsFrame()["frameId"], frame["frameId"]);
    EXPECT_EQ(xf->m_frameScheduler.GetState(), before);
}


TEST_F(XFramesQueueTest, TableRenderAppliesNumericSortAndTypedFilters) {
    xf->SetDiagnosticsEnabled(true);
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "di-table", {{"filterable", true}, {"clipRows", 10},
        {"style", {{"width", 600}, {"height", 350}}},
        {"columns", json::array({{{"fieldId", "value"}, {"heading", "Value"}, {"type", "number"}, {"defaultSort", true}},
                                {{"fieldId", "used"}, {"heading", "Used"}, {"type", "boolean"}}})}})});
    Internal(2, {{"op", "setData"}, {"data", json::array({{{"value", 20}, {"used", true}},
        {{"value", 3}, {"used", false}}, {{"value", 100}, {"used", true}}})}});
    Frame();
    Frame();
    auto state = Node(xf->GetDiagnosticsFrame(), 2)["state"];
    EXPECT_DOUBLE_EQ(std::stod(state["firstRow"]["value"].get<std::string>()), 3);
    EXPECT_DOUBLE_EQ(std::stod(state["lastRow"]["value"].get<std::string>()), 100);
    Internal(2, {{"op", "setColumnFilter"}, {"columnIndex", 1}, {"filterText", "Yes"}});
    Frame();
    EXPECT_EQ(Node(xf->GetDiagnosticsFrame(), 2)["state"]["filteredCount"], 2);
    Internal(2, {{"op", "setColumnFilter"}, {"columnIndex", 0}, {"filterText", "20"}});
    Frame();
    EXPECT_EQ(Node(xf->GetDiagnosticsFrame(), 2)["state"]["filteredCount"], 1);
}

TEST_F(XFramesQueueTest, MapCompletionAfterCaptureRemainsPendingAndDispatchesOutsideTreeLocks) {
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "map-view")});
    Frame();
    EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render);
    auto complete = MapProgressCompletion(2);
    xf->Render(900, 700);
    std::latch released(1);
    auto worker = std::async(std::launch::async, [&] { released.wait(); complete(); });
    released.count_down(); worker.get();
    const auto generation = xf->m_frameScheduler.GetState()["generation"];
    xf->CompleteDiagnosticsFrame();
    EXPECT_NE(xf->m_frameScheduler.GetState()["coveredGeneration"], generation);
    EXPECT_TRUE(xf->m_frameScheduler.TakeOpportunity().render);
    int callbacks = 0;
    xf->m_onGuardedPrefetchProgress = [&](auto source, int id, int done, int total) {
        EXPECT_TRUE(source.IsAlive()); EXPECT_EQ(id, 2); EXPECT_EQ(done, 1); EXPECT_EQ(total, 1);
        EXPECT_EQ(xf->GetDiagnosticsState()["elementCount"], 2); // requires released tree locks
        ++callbacks;
    };
    Frame(); xf->FlushResourceEvents();
    EXPECT_EQ(callbacks, 1);
    EXPECT_EQ(xf->m_frameScheduler.GetState()["coveredGeneration"], generation);
    EXPECT_FALSE(xf->m_frameScheduler.TakeOpportunity().render);
}

TEST_F(XFramesQueueTest, MapMovesPreserveRequestsAndRemovedCompletionsCannotReachReusedIds) {
    Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {}}}, {Create(2, "map-view"), Create(3)});
    auto* map = ElementAt(2);
    auto complete = MapProgressCompletion(2);
    Publish({{0, {1}}, {1, {3}}, {3, {2}}, {2, {}}});
    EXPECT_EQ(ElementAt(2), map);
    complete(); Frame(); // queues an event owned by the old Map lifetime
    auto late = MapProgressCompletion(2);
    Publish({{0, {1}}, {1, {}}});
    Publish({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "map-view")});
    Frame();
    int callbacks = 0;
    xf->m_onGuardedPrefetchProgress = [&](auto, int, int, int) { ++callbacks; };
    const auto before = xf->m_frameScheduler.GetState();
    late(); xf->FlushResourceEvents();
    EXPECT_EQ(xf->m_frameScheduler.GetState(), before);
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["resources"]["pendingRequests"], 0);
    Publish({{0, {1}}, {1, {}}}); Frame();
    EXPECT_EQ(xf->m_frameScheduler.GetState()["ownerCount"], 0);
}

TEST(MapWorkerTest, QueueIsBoundedAndCancellationReleasesQueuedLifetimesWithoutJoiningActiveCalls) {
    MapWorker worker;
    auto owner = std::make_shared<int>(1);
    std::latch running(MapWorker::WorkerCount), release(1), completed(MapWorker::WorkerCount);
    for (size_t i = 0; i < MapWorker::WorkerCount; ++i)
        EXPECT_TRUE(worker.Submit(owner, [&] { running.count_down(); release.wait(); completed.count_down(); }));
    running.wait();
    bool queuedRan = false;
    for (size_t i = 0; i < MapWorker::MaxQueued; ++i)
        EXPECT_TRUE(worker.Submit(owner, [&] { queuedRan = true; }));
    EXPECT_FALSE(worker.Submit(owner, [] {}));
    EXPECT_EQ(worker.Diagnostics()["queued"], MapWorker::MaxQueued);
    worker.Cancel(owner);
    EXPECT_EQ(worker.Diagnostics()["queued"], 0);
    worker.Stop();
    EXPECT_FALSE(worker.Submit(owner, [] {}));
    release.count_down(); completed.wait();
    EXPECT_FALSE(queuedRan);
}

TEST_F(XFramesQueueTest, RuntimeDisposalClosesResourcesWithoutInventingARevision) {
    Publish({{0, {1}}, {1, {2, 3, 4}}, {2, {}}, {3, {}}, {4, {}}}, {
        Create(2, "di-image", {{"url", "missing-on-shutdown.png"}}), Create(3, "di-js-canvas"), Create(4, "map-view")});
    auto oldSubject = CopyInternalSubject(3);
    auto complete = MapProgressCompletion(4);
    complete(); Frame();
    auto late = MapProgressCompletion(4);
    const auto before = xf->GetCommitState();
    const auto generation = xf->m_frameScheduler.GetState()["generation"];
    xf->Dispose();
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], before["nativeRevision"]);
    EXPECT_EQ(xf->GetCommitState()["nativeSequence"], before["nativeSequence"]);
    EXPECT_EQ(xf->GetCommitState()["initialized"], false);
    EXPECT_EQ(xf->GetCommitState()["surfaceStatus"], "disposed");
    EXPECT_EQ(xf->GetDiagnosticsState()["elementCount"], 0);
    EXPECT_EQ(xf->GetDiagnosticsState()["hierarchyCount"], 1);
    EXPECT_EQ(xf->GetDiagnosticsState()["internalSubjectCount"], 0);
    EXPECT_EQ(xf->m_frameScheduler.GetState()["ownerCount"], 0);
    EXPECT_EQ(xf->m_frameScheduler.GetStatus(), xframes::FrameScheduler::Status::Disposed);
    EXPECT_EQ(xf->GetDiagnosticsFrame()["resourceState"]["queuedPrefetchEvents"], 0);
    oldSubject.get_observer().on_next({{"op", "redraw"}});
    late(); xf->FlushResourceEvents(); xf->Dispose();
    EXPECT_EQ(xf->m_frameScheduler.GetState()["generation"], generation);
    auto result = xf->ApplyCommit(Wire().dump());
    ASSERT_TRUE(result.error); EXPECT_EQ(result.status, "rejected");
    EXPECT_EQ(result.error->code, "runtime_disposed");
    EXPECT_EQ(result.nativeRevision, std::stoull(before["nativeRevision"].get<std::string>()));
    EXPECT_FALSE(xf->Render(900, 700));
}

TEST_F(XFramesQueueTest, RendererCleanupIsPolymorphicAndIdempotent) {
    ImGuiRenderer* base = renderer.get();
    base->CleanUp();
    EXPECT_EQ(ImPlot::GetCurrentContext(), nullptr);
    EXPECT_EQ(renderer->m_imGuiCtx, nullptr);
    EXPECT_EQ(xf->GetCommitState()["surfaceStatus"], "disposed");
    EXPECT_TRUE(base->GetAvailableFonts().empty());
    base->CleanUp();
    EXPECT_EQ(base->GetResourceDiagnostics()["liveTextures"], 0);
}
