#include <gtest/gtest.h>
#include <thread>
#include <algorithm>
#include <barrier>
#include <future>
#include <filesystem>
#include <latch>
#include <map>
#include <random>
#include <set>
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
#include "widget/input_text.h"

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
    void SetCounters(uint64_t sequence, uint64_t revision) {
        xf->m_nativeSequence = sequence; xf->m_nativeRevision = revision; xf->m_visibleRevision = revision;
    }
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
// Re-linking a parent marks its Yoga subtree dirty, so the next frame lays it out
// again. A publication re-links only parents whose child list changed: a sibling
// subtree that did not change keeps its layout.
TEST_F(XFramesTest, PublicationRelinksOnlyParentsWhoseChildrenChanged) {
    Publish({{0, {1}}, {1, {2, 3}}, {2, {4}}, {3, {5}}, {4, {}}, {5, {}}},
        {Create(1), Create(2), Create(3), Create(4), Create(5)});
    const auto yoga = [&](int id) { return ElementAt(id)->m_layoutNode->m_node; };
    YGNodeCalculateLayout(yoga(1), 100, 100, YGDirectionLTR);
    ASSERT_FALSE(YGNodeIsDirty(yoga(3)));
    EXPECT_EQ(Publish({{0, {1}}, {1, {2, 3}}, {2, {6}}, {3, {5}}, {6, {}}, {5, {}}}, {Create(6)}).destroyedIds,
        (std::vector<int>{4}));
    EXPECT_TRUE(YGNodeIsDirty(yoga(2)));
    EXPECT_FALSE(YGNodeIsDirty(yoga(3)));
    EXPECT_FALSE(YGNodeIsDirty(yoga(5)));
    const auto state = xf->GetDiagnosticsState();
    EXPECT_EQ(Node(state, 2)["yogaChildren"], json::array({6}));
    EXPECT_EQ(Node(state, 3)["yogaChildren"], json::array({5}));
    EXPECT_EQ(Node(state, 1)["yogaChildren"], json::array({2, 3}));
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
        {Create(3, "di-table", {{"columns", json::array({{{"heading", "missing fieldId"}}})}}), "invalid_props"},
        {Create(3, "virtual-table", {{"rowCount", 10}}), "invalid_props"},
        {Create(3, "virtual-table", {{"columns", json::array({{{"heading", "A"}}})}, {"rowCount", -1}}), "invalid_props"},
        {Create(3, "virtual-table", {{"columns", json::array({{{"heading", "A"}}})}, {"rows", json::array({json::array({1})})}}), "invalid_props"}
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

// Events are stamped with the visible revision: the tree every frame drawn since
// the last applied publication shows. It moves only when a publication applies.
TEST_F(XFramesTest, VisibleRevisionFollowsAppliedPublicationsOnly) {
    EXPECT_EQ(xf->VisibleRevision(), 0u);
    Publish({{0, {1}}, {1, {}}}, {Create(1)});
    EXPECT_EQ(xf->VisibleRevision(), 1u);
    Reject(Wire({{0, {1}}, {1, {}}}, {Create(1)}), "duplicate_id", 0);
    EXPECT_EQ(xf->VisibleRevision(), 1u);
    Publish();
    EXPECT_EQ(xf->VisibleRevision(), 2u);
    EXPECT_EQ(std::to_string(xf->VisibleRevision()), xf->GetCommitState()["nativeRevision"].get<std::string>());
}

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

// "childLists": "changed" carries only the child lists that changed. An omitted
// list is the previous one; a created node without an assignment has none.
namespace {
json Changed(json wire) { wire["childLists"] = "changed"; return wire; }
}
TEST_F(XFramesTest, ChangedChildListsAreAdvertised) {
    const auto features = xf->GetCommitState()["commitFeatures"];
    ASSERT_TRUE(features.is_array());
    EXPECT_NE(std::find(features.begin(), features.end(), "changedChildLists"), features.end());
}
TEST_F(XFramesTest, ChangedChildListsKeepOmittedListsAndStartCreatedNodesEmpty) {
    Publish({{0, {1}}, {1, {2, 3}}, {2, {}}, {3, {}}}, {Create(1), Create(2), Create(3)});
    // A patch alone: nothing re-assigned, nothing destroyed.
    auto patched = xf->ApplyCommit(Changed(Wire({{0, {1}}}, {Patch(2, {{"style", {{"width", 5}}}})})).dump());
    ASSERT_EQ(patched.status, "applied") << patched.ToJson();
    EXPECT_TRUE(patched.destroyedIds.empty());
    EXPECT_EQ(xf->GetChildren(1), (std::vector<int>{2, 3}));
    // A created leaf needs no assignment of its own.
    auto inserted = xf->ApplyCommit(Changed(Wire({{0, {1}}, {1, {2, 4, 3}}}, {Create(4)})).dump());
    ASSERT_EQ(inserted.status, "applied") << inserted.ToJson();
    EXPECT_EQ(xf->GetChildren(1), (std::vector<int>{2, 4, 3}));
    EXPECT_EQ(xf->GetChildren(4), std::vector<int>{});
    EXPECT_EQ(xf->GetCommitState()["managedCount"], 4);
    // Removing a subtree destroys it, descendants first.
    Publish({{0, {1}}, {1, {2, 4, 3}}, {2, {5}}, {4, {}}, {3, {}}, {5, {}}}, {Create(5)});
    auto removed = xf->ApplyCommit(Changed(Wire({{0, {1}}, {1, {4, 3}}})).dump());
    ASSERT_EQ(removed.status, "applied") << removed.ToJson();
    EXPECT_EQ(removed.destroyedIds, (std::vector<int>{5, 2}));
    EXPECT_EQ(xf->GetCommitState()["managedCount"], 3);
}
TEST_F(XFramesTest, ChangedChildListsMoveOutOfARemovedAncestor) {
    Publish({{0, {1, 4}}, {1, {2}}, {2, {3}}, {3, {}}, {4, {}}}, {Create(1), Create(2), Create(3), Create(4)});
    auto moved = ElementAt(3);
    auto result = xf->ApplyCommit(Changed(Wire({{0, {4}}, {4, {3}}})).dump());
    ASSERT_EQ(result.status, "applied") << result.ToJson();
    EXPECT_EQ(result.destroyedIds, (std::vector<int>{2, 1}));
    EXPECT_EQ(ElementAt(3), moved);
    EXPECT_EQ(xf->GetChildren(4), (std::vector<int>{3}));
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 3)["yogaParent"], 4);
}
TEST_F(XFramesTest, ChangedChildListsValidateTheFinalTree) {
    InsertUnowned(99);
    Publish({{0, {1, 4}}, {1, {2, 5}}, {2, {3}}, {3, {}}, {4, {}}, {5, {}}},
        {Create(1), Create(2), Create(3), Create(4), Create(5, "unformatted-text", {{"text", "leaf"}})});
    // A child still listed by an unchanged, surviving parent cannot be listed again.
    Reject(Changed(Wire({{0, {1, 4}}, {4, {2}}})), "multiple_parents");
    // A cycle through unchanged links: 1 -> 2 -> 3 (unchanged) -> 1.
    Reject(Changed(Wire({{0, {4}}, {3, {1}}})), "cycle");
    Reject(Changed(Wire({{0, {1, 4}}}, {Create(6)})), "unreachable_operation", 0);
    Reject(Changed(Wire({{0, {4}}}, {Patch(2, json::object())})), "unreachable_operation", 0);
    Reject(Changed(Wire({{0, {1, 4}}, {5, {6}}}, {Create(6)})), "invalid_relationship");
    Reject(Changed(Wire({{0, {1, 4}}, {4, {6}}}, {Create(6, "node", {{"root", true}})})), "invalid_relationship");
    Reject(Changed(Wire({{0, {1, 4}}, {4, {}}}, {Children(4, {})})), "duplicate_assignment");
    Reject(Changed(Wire({{0, {1, 4}}, {4, {99}}})), "ownership_conflict");
    Reject(Changed(Wire({{0, {1, 4}}, {4, {77}}})), "missing_target");
    Reject(Changed(Wire({{0, {1, 4}}, {4, {6, 6}}}, {Create(6)})), "duplicate_child");
    for (auto value : json::array({"partial", 1, nullptr, true})) {
        auto wire = Wire({{0, {1, 4}}}); wire["childLists"] = value; Reject(wire, "invalid_field");
    }
    auto explicitComplete = Wire({{0, {1, 4}}, {1, {2, 5}}, {2, {3}}, {3, {}}, {4, {}}, {5, {}}});
    explicitComplete["childLists"] = "complete";
    EXPECT_EQ(xf->ApplyCommit(explicitComplete.dump()).status, "applied");
    // "complete" still requires every list.
    auto incomplete = Wire({{0, {1, 4}}}); incomplete["childLists"] = "complete";
    Reject(incomplete, "missing_children");
}
TEST_F(XFramesTest, ChangedChildListsLeaveUnchangedLayoutClean) {
    std::vector<int> rows;
    std::vector<json> creates{Create(1)};
    for (int id = 2; id <= 2001; ++id) { rows.push_back(id); creates.push_back(Create(id)); }
    auto wire = Wire({{0, {1}}}, {}); wire["childLists"] = "changed";
    for (const auto& create : creates) wire["operations"].push_back(create);
    wire["operations"].push_back(Children(1, rows));
    ASSERT_EQ(xf->ApplyCommit(wire.dump()).status, "applied");
    const auto yoga = [&](int id) { return ElementAt(id)->m_layoutNode->m_node; };
    YGNodeCalculateLayout(yoga(1), 100, 100, YGDirectionLTR);
    auto patch = xf->ApplyCommit(Changed(Wire({{0, {1}}}, {Patch(700, json::object())})).dump());
    ASSERT_EQ(patch.status, "applied") << patch.ToJson();
    EXPECT_FALSE(YGNodeIsDirty(yoga(1)));
    EXPECT_FALSE(YGNodeIsDirty(yoga(700)));
    EXPECT_EQ(xf->GetChildren(1).size(), 2000u);
}

// Two surfaces, one fed complete commits and one fed changed commits for the same
// seeded edits, must stay identical: hierarchy, Yoga links, owned nodes, revisions
// and destroyed sets. Changed destruction is descendant-first in the previous tree.
TEST_F(XFramesTest, ChangedChildListsEqualCompleteCommitsOverSeededEdits) {
    for (unsigned seed : {1u, 7u, 2026u}) {
        auto complete = std::make_unique<XFrames>("publication-test", std::nullopt);
        auto changed = std::make_unique<XFrames>("publication-test", std::nullopt);
        std::mt19937 random(seed);
        const auto pick = [&](size_t n) { return std::uniform_int_distribution<size_t>(0, n - 1)(random); };
        std::map<int, std::vector<int>> tree{{0, {}}};
        int next = 1;
        const auto subtree = [&](int id) {
            std::vector<int> result, pending{id};
            while (!pending.empty()) {
                int at = pending.back(); pending.pop_back(); result.push_back(at);
                for (int child : tree.at(at)) pending.push_back(child);
            }
            return result;
        };
        const auto parentOf = [&](int id) {
            for (const auto& [parent, children] : tree)
                if (std::find(children.begin(), children.end(), id) != children.end()) return parent;
            return -1;
        };
        for (int step = 0; step < 200; ++step) {
            const auto before = tree;
            std::vector<json> creates;
            std::vector<int> nodes;
            for (const auto& [id, _] : tree) nodes.push_back(id);
            const int at = nodes[pick(nodes.size())];
            switch (pick(5)) {
            case 0: case 1: { // insert one or two new nodes
                for (int k = 0; k <= static_cast<int>(pick(2)); ++k) {
                    auto& children = tree.at(at);
                    children.insert(children.begin() + pick(children.size() + 1), next);
                    tree[next] = {};
                    creates.push_back(Create(next++));
                }
                break;
            }
            case 2: { // remove a subtree, first moving one grandchild out of it
                if (at == 0) break;
                const auto doomed = subtree(at);
                for (int id : doomed) {
                    if (id != at && pick(2) == 0) {
                        auto& old = tree.at(parentOf(id));
                        old.erase(std::find(old.begin(), old.end(), id));
                        tree.at(0).push_back(id);
                        break;
                    }
                }
                auto& siblings = tree.at(parentOf(at));
                siblings.erase(std::find(siblings.begin(), siblings.end(), at));
                for (int id : subtree(at)) tree.erase(id);
                break;
            }
            case 3: { // move a node under another parent outside its subtree
                if (at == 0) break;
                const auto inside = subtree(at);
                std::vector<int> targets;
                for (int id : nodes) if (std::find(inside.begin(), inside.end(), id) == inside.end()) targets.push_back(id);
                const int target = targets[pick(targets.size())];
                auto& old = tree.at(parentOf(at));
                old.erase(std::find(old.begin(), old.end(), at));
                auto& children = tree.at(target);
                children.insert(children.begin() + pick(children.size() + 1), at);
                break;
            }
            default: // reorder one child list
                std::shuffle(tree.at(at).begin(), tree.at(at).end(), random);
            }
            json full = {{"schemaVersion", 2}, {"surfaceId", 0}, {"baseRevision", complete->GetCommitState()["nativeRevision"]},
                {"rootChildren", tree.at(0)}, {"operations", creates}};
            json delta = full;
            delta["childLists"] = "changed";
            for (const auto& [id, children] : tree) {
                if (id == 0) continue;
                full["operations"].push_back(Children(id, children));
                const auto old = before.find(id);
                if (old == before.end() ? !children.empty() : old->second != children)
                    delta["operations"].push_back(Children(id, children));
            }
            const auto a = complete->ApplyCommit(full.dump());
            const auto b = changed->ApplyCommit(delta.dump());
            ASSERT_EQ(a.status, "applied") << seed << ":" << step << " " << a.ToJson();
            ASSERT_EQ(b.status, "applied") << seed << ":" << step << " " << b.ToJson() << delta;
            EXPECT_EQ(a.nativeRevision, b.nativeRevision);
            EXPECT_EQ(std::set<int>(a.destroyedIds.begin(), a.destroyedIds.end()),
                std::set<int>(b.destroyedIds.begin(), b.destroyedIds.end())) << seed << ":" << step;
            for (size_t i = 0; i < b.destroyedIds.size(); ++i) {
                const auto prior = before.find(b.destroyedIds[i]);
                ASSERT_NE(prior, before.end());
                for (int child : prior->second)
                    for (size_t j = i + 1; j < b.destroyedIds.size(); ++j)
                        EXPECT_NE(b.destroyedIds[j], child) << "descendant destroyed after its parent, " << seed << ":" << step;
            }
            ASSERT_EQ(complete->GetDiagnosticsState().dump(), changed->GetDiagnosticsState().dump()) << seed << ":" << step;
            for (const auto& [id, children] : tree) ASSERT_EQ(changed->GetChildren(id), children) << seed << ":" << step;
            ASSERT_EQ(changed->GetCommitState()["managedCount"], tree.size() - 1);
        }
    }
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

TEST_F(XFramesQueueTest, ApplicationDiagnosticsReportRetainedHistoryAndMapOverlays) {
    Publish({{0, {1}}, {1, {2, 3, 4}}, {2, {}}, {3, {}}, {4, {}}}, {
        Create(2, "plot-line", {{"dataPointsLimit", 2}}),
        Create(3, "plot-scatter", {{"dataPointsLimit", 2}}), Create(4, "map-view"),
    });
    Internal(4, {{"op", "setPolylines"}, {"polylines", json::array({{{"points", json::array()}, {"pointsLimit", 2}}})}});
    Internal(4, {{"op", "setMarkers"}, {"markers", json::array({{{"lat", 51.5}, {"lon", -0.12}}})}});
    Internal(4, {{"op", "setOverlays"}, {"overlays", json::array({{{"lat", 51.5}, {"lon", -0.12}, {"radiusMeters", 30}}})}});
    for (int i = 0; i < 5; ++i) {
        Internal(2, {{"op", "appendData"}, {"x", i}, {"y", i}});
        Internal(3, {{"op", "appendData"}, {"x", i}, {"y", i}});
        Internal(4, {{"op", "appendPolylinePoint"}, {"polylineIndex", 0}, {"lat", 51.5}, {"lon", -0.12 + i * .001}});
    }
    Frame();
    const auto state = xf->GetDiagnosticsState();
    EXPECT_EQ(Node(state, 2)["state"]["pointCount"], 2);
    EXPECT_EQ(Node(state, 3)["state"]["pointCount"], 2);
    const auto map = Node(state, 4)["state"];
    EXPECT_EQ(map["polylinePoints"], 2);
    EXPECT_EQ(map["firstPolylineLimit"], 2);
    EXPECT_EQ(map["markerCount"], 1);
    EXPECT_EQ(map["overlayCount"], 1);
    EXPECT_EQ(map["lastMarker"], (json{{"lat", 51.5}, {"lon", -0.12}}));
    EXPECT_EQ(map["lastOverlayRadiusMeters"], 30);
}

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
