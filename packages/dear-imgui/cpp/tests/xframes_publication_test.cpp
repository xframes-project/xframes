// Commits and publication: the wire, its validation and the published tree, without drawing.
#include "xframes_test_support.h"

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
// ImGui pushes a style variable only in its own shape, one float or a pair, and a widget pops as
// many as it holds, so a value of the wrong shape would pop a variable the widget never pushed.
TEST_F(XFramesTest, PublicationRejectsAStyleVarOfTheWrongShape) {
    Publish({{0, {1}}, {1, {}}}, {Create(1)});
    const auto style = [](ImGuiStyleVar var, json value) { return json{{"vars", {{std::to_string(var), value}}}}; };
    for (const auto& family : {"style", "hoverStyle"})
        for (const auto& value : {style(ImGuiStyleVar_FramePadding, 4), style(ImGuiStyleVar_Alpha, json::array({0.5, 0.5}))})
            Reject(Wire({{0, {1}}, {1, {2}}, {2, {}}}, {Create(2, "node", {{family, value}})}), "invalid_props", 0);
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
