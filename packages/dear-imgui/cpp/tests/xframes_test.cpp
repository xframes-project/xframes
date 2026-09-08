#include <gtest/gtest.h>
#include <thread>
#include <algorithm>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <rpp/rpp.hpp>
#include "widget/styled_widget.h"
#include "xframes.h"
#include "element/element.h"
#include "implot_renderer.h"
#include "widget/table.h"

using json = nlohmann::json;
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::IsFalse;
using ::testing::IsEmpty;

// Test fixture that directly manipulates XFrames maps, bypassing the RPP
// reactive queue and avoiding ImGui/GLFW/OpenGL dependencies entirely.
class XFramesTest : public ::testing::Test {
protected:
    std::unique_ptr<XFrames> xf;

    void SetUp() override {
        xf = std::make_unique<XFrames>("test_window", std::nullopt);
        // Do NOT call SetUpSubjects() — tests bypass the reactive queue.
        // The default-constructed RPP subject destructs cleanly.
        InsertNode(0);
    }

    // Insert a plain Element directly into the registries
    void InsertNode(int id) {
        auto element = std::make_unique<Element>(nullptr, id, id == 0, false, false);
        json emptyDef = json::object();
        element->m_layoutNode->ApplyStyle(emptyDef);
        xf->m_elements[id] = std::move(element);
        xf->m_hierarchy[id] = std::vector<int>();
    }

    // Wire parent-child in both hierarchy map and Yoga tree
    void LinkChildren(int parentId, const std::vector<int>& childIds) {
        xf->m_hierarchy[parentId] = childIds;
        YGNodeRemoveAllChildren(xf->m_elements[parentId]->m_layoutNode->m_node);
        for (size_t i = 0; i < childIds.size(); i++) {
            xf->m_elements[parentId]->m_layoutNode->InsertChild(
                xf->m_elements[childIds[i]]->m_layoutNode.get(), i);
        }
    }

    // Call the private SetChildren with proper JSON (tests the real orphan cleanup path)
    void CallSetChildren(int parentId, const std::vector<int>& childIds) {
        json opDef;
        opDef["parentId"] = parentId;
        opDef["childrenIds"] = childIds;
        xf->SetChildren(opDef);
    }

    bool HasElement(int id) const {
        return xf->m_elements.contains(id);
    }

    bool HasHierarchyEntry(int id) const {
        return xf->m_hierarchy.contains(id);
    }

    std::vector<int> GetHierarchyChildren(int id) const {
        if (xf->m_hierarchy.contains(id)) {
            return xf->m_hierarchy[id];
        }
        return {};
    }

    bool HasInternalOpsSubject(int id) const {
        return xf->m_elementInternalOpsSubject.contains(id);
    }

    void InjectInternalOpsSubject(int id) {
        xf->m_elementInternalOpsSubject[id] = rpp::subjects::serialized_replay_subject<json>{10};
    }

    size_t ElementCount() const {
        return xf->m_elements.size();
    }

    size_t FloatFormatCharsCount() const {
        return xf->m_floatFormatChars.size();
    }

    bool HasFloatFormatChar(int key) const {
        return xf->m_floatFormatChars.contains(key);
    }

    void DirectRemoveElement(int id) {
        xf->RemoveElement(id);
    }

    auto CopyInternalSubject(int id) {
        return xf->m_elementInternalOpsSubject.at(id);
    }

    void SetCommitCounters(uint64_t sequence, uint64_t revision) {
        xf->m_nativeSequence = sequence;
        xf->m_nativeRevision = revision;
    }

    void FailButtonCreation() {
        xf->m_element_init_fn["di-button"] = [](const json&, std::optional<WidgetStyle>, XFrames*) -> std::unique_ptr<Element> {
            throw std::runtime_error("injected constructor failure");
        };
    }

    bool LastCommitRequestExpired() {
        bool expired = false;
        auto subscription = rpp::composite_disposable_wrapper::make();
        xf->m_elementOpSubject.get_observable() | rpp::ops::subscribe(subscription,
            [&](const std::weak_ptr<CommitRequest>& request) { expired = request.expired(); });
        subscription.dispose();
        return expired;
    }
};

// --- RemoveElement tests ---

TEST_F(XFramesTest, RemoveElement_SingleNode) {
    InsertNode(1);
    LinkChildren(0, {1});

    ASSERT_THAT(HasElement(1), IsTrue());
    ASSERT_THAT(HasHierarchyEntry(1), IsTrue());

    DirectRemoveElement(1);

    EXPECT_THAT(HasElement(1), IsFalse());
    EXPECT_THAT(HasHierarchyEntry(1), IsFalse());
}

TEST_F(XFramesTest, RemoveElement_RecursivelyRemovesChildren) {
    InsertNode(1);
    InsertNode(2);
    InsertNode(3);
    LinkChildren(0, {1});
    LinkChildren(1, {2});
    LinkChildren(2, {3});

    DirectRemoveElement(1);

    EXPECT_THAT(HasElement(1), IsFalse());
    EXPECT_THAT(HasElement(2), IsFalse());
    EXPECT_THAT(HasElement(3), IsFalse());
    EXPECT_THAT(HasHierarchyEntry(1), IsFalse());
    EXPECT_THAT(HasHierarchyEntry(2), IsFalse());
    EXPECT_THAT(HasHierarchyEntry(3), IsFalse());
}

TEST_F(XFramesTest, RemoveElement_DoesNotAffectSiblings) {
    InsertNode(1);
    InsertNode(2);
    LinkChildren(0, {1, 2});

    DirectRemoveElement(1);

    EXPECT_THAT(HasElement(1), IsFalse());
    EXPECT_THAT(HasElement(2), IsTrue());
    EXPECT_THAT(HasHierarchyEntry(2), IsTrue());
}

TEST_F(XFramesTest, RemoveElement_NonExistentId) {
    DirectRemoveElement(999);

    // Root element still intact
    EXPECT_THAT(HasElement(0), IsTrue());
}

TEST_F(XFramesTest, RemoveElement_DoesNotCorruptFloatFormatChars) {
    ASSERT_THAT(FloatFormatCharsCount(), Eq(10u));
    ASSERT_THAT(HasFloatFormatChar(3), IsTrue());

    InsertNode(3);
    LinkChildren(0, {3});

    DirectRemoveElement(3);

    // All 10 global format strings must survive
    EXPECT_THAT(FloatFormatCharsCount(), Eq(10u));
    EXPECT_THAT(HasFloatFormatChar(3), IsTrue());
}

TEST_F(XFramesTest, RemoveElement_ErasesInternalOpsSubject) {
    InsertNode(5);
    LinkChildren(0, {5});

    InjectInternalOpsSubject(5);
    ASSERT_THAT(HasInternalOpsSubject(5), IsTrue());

    DirectRemoveElement(5);

    EXPECT_THAT(HasInternalOpsSubject(5), IsFalse());
}

// --- SetChildren orphan cleanup tests ---

TEST_F(XFramesTest, SetChildren_OrphanedChildrenAreRemoved) {
    InsertNode(1);
    InsertNode(2);
    InsertNode(3);
    LinkChildren(0, {1, 2, 3});

    CallSetChildren(0, {1}); // orphans 2 and 3

    EXPECT_THAT(HasElement(1), IsTrue());
    EXPECT_THAT(HasElement(2), IsFalse());
    EXPECT_THAT(HasElement(3), IsFalse());
    EXPECT_THAT(GetHierarchyChildren(0), Eq(std::vector<int>{1}));
}

TEST_F(XFramesTest, SetChildren_NonOrphanedChildrenPreserved) {
    InsertNode(1);
    InsertNode(2);
    InsertNode(3);
    InsertNode(4);
    LinkChildren(0, {1, 2, 3});

    CallSetChildren(0, {2, 3, 4}); // 1 orphaned, 2+3 preserved, 4 added

    EXPECT_THAT(HasElement(1), IsFalse());
    EXPECT_THAT(HasElement(2), IsTrue());
    EXPECT_THAT(HasElement(3), IsTrue());
    EXPECT_THAT(HasElement(4), IsTrue());
    EXPECT_THAT(GetHierarchyChildren(0), Eq(std::vector<int>{2, 3, 4}));
}

TEST_F(XFramesTest, SetChildren_EmptyNewList) {
    InsertNode(1);
    InsertNode(2);
    LinkChildren(0, {1, 2});

    CallSetChildren(0, {});

    EXPECT_THAT(HasElement(1), IsFalse());
    EXPECT_THAT(HasElement(2), IsFalse());
    EXPECT_THAT(GetHierarchyChildren(0), IsEmpty());
}

TEST_F(XFramesTest, SetChildren_CompleteReplacement) {
    InsertNode(1);
    InsertNode(2);
    InsertNode(3);
    InsertNode(4);
    LinkChildren(0, {1, 2});

    CallSetChildren(0, {3, 4});

    EXPECT_THAT(HasElement(1), IsFalse());
    EXPECT_THAT(HasElement(2), IsFalse());
    EXPECT_THAT(HasElement(3), IsTrue());
    EXPECT_THAT(HasElement(4), IsTrue());
    EXPECT_THAT(GetHierarchyChildren(0), Eq(std::vector<int>{3, 4}));
}

TEST_F(XFramesTest, SetChildren_OrphanedSubtreeRecursivelyRemoved) {
    InsertNode(1);
    InsertNode(2);
    InsertNode(3);
    LinkChildren(0, {1});
    LinkChildren(1, {2});
    LinkChildren(2, {3});

    CallSetChildren(0, {});

    EXPECT_THAT(HasElement(1), IsFalse());
    EXPECT_THAT(HasElement(2), IsFalse());
    EXPECT_THAT(HasElement(3), IsFalse());
    EXPECT_THAT(HasHierarchyEntry(1), IsFalse());
    EXPECT_THAT(HasHierarchyEntry(2), IsFalse());
    EXPECT_THAT(HasHierarchyEntry(3), IsFalse());
}

TEST_F(XFramesTest, SetChildren_IdenticalList) {
    InsertNode(1);
    InsertNode(2);
    LinkChildren(0, {1, 2});

    auto countBefore = ElementCount();

    CallSetChildren(0, {1, 2});

    EXPECT_THAT(HasElement(1), IsTrue());
    EXPECT_THAT(HasElement(2), IsTrue());
    EXPECT_THAT(ElementCount(), Eq(countBefore));
}

// These tests use the real serialized subject handlers and real ImGui/ImPlot
// frame construction. No window, graphics driver, or fake native tree is involved.
class XFramesQueueTest : public XFramesTest {
protected:
    std::unique_ptr<ImPlotRenderer> renderer;

    void SetUp() override {
        xf = std::make_unique<XFrames>("queue-test", std::nullopt);
        std::string fonts = "{}";
        renderer = std::make_unique<ImPlotRenderer>(xf.get(), "queue-test", "queue-test", fonts, std::nullopt);
        auto& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(900, 700);
        io.DeltaTime = 1.0f / 60;
        io.FontDefault = io.Fonts->AddFontDefault();
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        xf->m_onInit = [] {};
        xf->m_onTableSort = [](int, int, int) {};
        xf->m_onTableFilter = [](int, int, const std::string&) {};
        xf->m_onTableRowClick = [](int, int) {};
        xf->Init(renderer.get());
        Create({{"id", 1}, {"type", "node"}, {"root", true}, {"style", {{"width", 850}, {"height", 650}}}});
        xf->QueueSetChildren(0, {1});
    }

    void TearDown() override {
        xf.reset();
        ImPlot::DestroyContext();
        ImGui::DestroyContext(renderer->m_imGuiCtx);
        renderer.reset();
    }

    void Create(const json& definition) {
        auto payload = definition.dump();
        xf->QueueCreateElement(payload);
    }

    void Internal(int id, const json& operation) {
        auto payload = operation.dump();
        xf->QueueElementInternalOp(id, payload);
    }

    void Frame() {
        xf->Render(900, 700);
        // No GPU submission in this fixture; exercises the publication hook explicitly.
        xf->CompleteDiagnosticsFrame();
    }

    json Node(const json& state, int id) {
        for (const auto& node : state["elements"]) if (node["id"] == id) return node;
        return nullptr;
    }
};

TEST_F(XFramesQueueTest, QueueMaintainsHierarchyAndYogaOwnership) {
    Create({{"id", 2}, {"type", "node"}});
    Create({{"id", 3}, {"type", "node"}});
    Create({{"id", 4}, {"type", "node"}});
    xf->QueueAppendChild(1, 2);
    xf->QueueAppendChild(1, 3);
    xf->QueueAppendChild(2, 4);
    auto state = xf->GetDiagnosticsState();
    EXPECT_EQ(state["elementCount"], 4);
    EXPECT_EQ(state["unreachableCount"], 0);
    EXPECT_EQ(Node(state, 1)["children"], json::array({2, 3}));
    EXPECT_EQ(Node(state, 2)["yogaChildren"], json::array({4}));
    EXPECT_EQ(Node(state, 4)["yogaParent"], 2);
    xf->QueueSetChildren(1, {3, 2});
    state = xf->GetDiagnosticsState();
    EXPECT_EQ(Node(state, 1)["yogaChildren"], json::array({3, 2}));
    xf->QueueSetChildren(1, {3});
    state = xf->GetDiagnosticsState();
    EXPECT_EQ(state["elementCount"], 2);
    EXPECT_TRUE(Node(state, 2).is_null());
    EXPECT_TRUE(Node(state, 4).is_null());
}

TEST_F(XFramesQueueTest, ImperativeDataAndSubjectsUseRealDelivery) {
    xf->SetDiagnosticsEnabled(true);
    Create({{"id", 2}, {"type", "plot-bar"}, {"series", json::array({{{"label", "A"}}, {{"label", "B"}}})}});
    Create({{"id", 3}, {"type", "di-table"}, {"columns", json::array({{{"fieldId", "value"}, {"heading", "Value"}, {"type", "number"}}})}});
    xf->QueueSetChildren(1, {2, 3});
    Internal(2, {{"op", "appendSeriesData"}, {"seriesIndex", 1}, {"x", 42}, {"y", 100}});
    Internal(3, {{"op", "setData"}, {"data", json::array({{{"value", 42}}})}});
    auto state = xf->GetDiagnosticsState();
    EXPECT_EQ(state["internalSubjectCount"], 2);
    EXPECT_EQ(Node(state, 2)["state"]["series"][1]["lastX"], 42);
    EXPECT_EQ(Node(state, 3)["state"]["rowCount"], 1);
    EXPECT_DOUBLE_EQ(std::stod(Node(state, 3)["state"]["firstRow"]["value"].get<std::string>()), 42);
    EXPECT_GT(Node(state, 2)["lastInternalOpMs"].get<double>(), 0);
    xf->QueueSetChildren(1, {});
    state = xf->GetDiagnosticsState();
    EXPECT_EQ(state["internalSubjectCount"], 0);
    EXPECT_EQ(state["elementCount"], 1);
    Internal(2, {{"op", "appendData"}, {"x", 43}, {"y", 0}});
    EXPECT_EQ(xf->GetDiagnosticsState()["elementCount"], 1);
}

TEST_F(XFramesQueueTest, FrameDiagnosticsAreOptInAndPublishTheConstructedState) {
    Frame();
    EXPECT_EQ(xf->GetDiagnosticsFrame()["frame"], 0);
    xf->SetDiagnosticsEnabled(true);
    xf->Render(900, 700);
    // Mutations after construction must not be misattributed to that frame.
    Create({{"id", 2}, {"type", "node"}});
    xf->QueueAppendChild(1, 2);
    EXPECT_EQ(xf->GetDiagnosticsFrame()["frame"], 0);
    xf->CompleteDiagnosticsFrame();
    auto frame = xf->GetDiagnosticsFrame();
    EXPECT_EQ(frame["frame"], 1);
    EXPECT_EQ(frame["elementCount"], 1);
    EXPECT_GE(frame["submittedAtMs"].get<double>(), frame["constructedAtMs"].get<double>());
    Frame();
    frame = xf->GetDiagnosticsFrame();
    EXPECT_EQ(frame["frame"], 2);
    EXPECT_EQ(frame["elementCount"], 2);
    xf->SetDiagnosticsEnabled(false);
    Frame();
    EXPECT_EQ(xf->GetDiagnosticsFrame()["frame"], 2);
    EXPECT_EQ(xf->GetDiagnosticsFrame()["enabled"], false);
    // An unsubmitted frame must not be published after disabling diagnostics.
    xf->SetDiagnosticsEnabled(true);
    xf->Render(900, 700);
    xf->SetDiagnosticsEnabled(false);
    Frame();
    EXPECT_EQ(xf->GetDiagnosticsFrame()["frame"], 2);
}

TEST_F(XFramesQueueTest, RepeatedQueueLifecycleReturnsNativeCountsToBaseline) {
    const auto baseline = xf->GetDiagnosticsState();
    for (int cycle = 0; cycle < 1000; ++cycle) {
        Create({{"id", 2}, {"type", "node"}});
        Create({{"id", 3}, {"type", "plot-bar"}});
        xf->QueueAppendChild(2, 3);
        xf->QueueSetChildren(1, {2});
        Internal(3, {{"op", "appendData"}, {"x", cycle}, {"y", 1}});
        xf->QueueSetChildren(1, {});
        const auto state = xf->GetDiagnosticsState();
        ASSERT_EQ(state["elementCount"], baseline["elementCount"]) << cycle;
        ASSERT_EQ(state["hierarchyCount"], baseline["hierarchyCount"]) << cycle;
        ASSERT_EQ(state["internalSubjectCount"], baseline["internalSubjectCount"]) << cycle;
    }
}

TEST_F(XFramesQueueTest, TableRenderAppliesNumericSortAndTypedFilters) {
    xf->SetDiagnosticsEnabled(true);
    Create({{"id", 2}, {"type", "di-table"}, {"filterable", true}, {"clipRows", 10},
        {"style", {{"width", 600}, {"height", 350}}},
        {"columns", json::array({{{"fieldId", "value"}, {"heading", "Value"}, {"type", "number"}, {"defaultSort", true}},
                                {{"fieldId", "used"}, {"heading", "Used"}, {"type", "boolean"}}})}});
    xf->QueueSetChildren(1, {2});
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

TEST_F(XFramesQueueTest, CharacterizeSameIdReparentDefect) {
    Create({{"id", 2}, {"type", "node"}});
    Create({{"id", 3}, {"type", "node"}});
    Create({{"id", 4}, {"type", "plot-bar"}});
    xf->QueueSetChildren(1, {2, 3});
    xf->QueueAppendChild(2, 4);
    Internal(4, {{"op", "appendData"}, {"x", 12}, {"y", 4}});
    xf->QueueSetChildren(2, {});
    xf->QueueSetChildren(3, {4});
    auto state = xf->GetDiagnosticsState();
    const bool reparentInvariant = !Node(state, 4).is_null();
    EXPECT_FALSE(reparentInvariant) << "XPASS XF-LIFE-005: remove the expected failure after reparenting is fixed";
    EXPECT_EQ(state["elementCount"], 3) << "XF-LIFE-005 signature changed";
    EXPECT_EQ(Node(state, 3)["children"], json::array({4}));
    EXPECT_EQ(Node(state, 3)["yogaChildren"], json::array());
    xf->QueueSetChildren(0, {});
    state = xf->GetDiagnosticsState();
    EXPECT_EQ(state["elementCount"], 0);
    EXPECT_EQ(state["unreachableCount"], 0);
    RecordProperty("known_defects", "XF-LIFE-005");
}

TEST_F(XFramesQueueTest, VirtualContainerAcknowledgesDeepDestructionAndPreservesSurvivors) {
    Create({{"id", 2}, {"type", "node"}, {"root", true}});
    Create({{"id", 3}, {"type", "plot-bar"}});
    Create({{"id", 4}, {"type", "di-table"}, {"columns", json::array({{{"fieldId", "value"}, {"heading", "Value"}, {"type", "number"}}})}});
    xf->QueueSetChildren(1, {3});
    xf->QueueSetChildren(2, {4});
    EXPECT_TRUE(xf->QueueSetChildren(0, {1, 2}).empty());
    Internal(4, {{"op", "setData"}, {"data", json::array({{{"value", 42}}})}});
    const auto survivor = Node(xf->GetDiagnosticsState(), 4);
    EXPECT_TRUE(xf->QueueSetChildren(0, {2, 1}).empty());
    EXPECT_EQ(xf->QueueSetChildren(0, {2}), (std::vector<int>{3, 1}));
    const auto state = xf->GetDiagnosticsState();
    EXPECT_EQ(state["elementCount"], 2);
    EXPECT_EQ(state["hierarchyCount"], 3);
    EXPECT_EQ(state["internalSubjectCount"], 1);
    EXPECT_EQ(Node(state, 4)["state"], survivor["state"]);
    EXPECT_EQ(Node(state, 2)["yogaChildren"], json::array({4}));
    EXPECT_EQ(Node(state, 4)["yogaParent"], 2);
    EXPECT_EQ(xf->QueueSetChildren(0, {}), (std::vector<int>{4, 2}));
    EXPECT_TRUE(xf->QueueSetChildren(0, {}).empty());
    EXPECT_TRUE(xf->QueueSetChildren(2, {}).empty());
    EXPECT_TRUE(xf->GetChildren(2).empty()); // a stale read must not recreate metadata
    Internal(4, {{"op", "setData"}, {"data", json::array()}});
    EXPECT_FALSE(xf->IsElementAlive(4));
    EXPECT_EQ(xf->GetDiagnosticsState()["elementCount"], 0);
    EXPECT_EQ(xf->GetDiagnosticsState()["hierarchyCount"], 1);
    EXPECT_EQ(xf->GetDiagnosticsState()["internalSubjectCount"], 0);
}

TEST_F(XFramesQueueTest, ThousandPopulatedRootUnmountsReturnExactLifetimeResults) {
    EXPECT_EQ(xf->QueueSetChildren(0, {}), (std::vector<int>{1}));
    const auto baseline = xf->GetDiagnosticsState();
    for (int cycle = 0; cycle < 1000; ++cycle) {
        Create({{"id", 1}, {"type", "node"}, {"root", true}});
        Create({{"id", 2}, {"type", "plot-bar"}});
        Create({{"id", 3}, {"type", "di-table"}, {"columns", json::array({{{"fieldId", "value"}, {"heading", "Value"}, {"type", "number"}}})}});
        xf->QueueSetChildren(1, {2, 3});
        xf->QueueSetChildren(0, {1});
        Internal(2, {{"op", "appendData"}, {"x", cycle}, {"y", 7}});
        ASSERT_TRUE(xf->IsElementAlive(2));
        ASSERT_EQ(Node(xf->GetDiagnosticsState(), 2)["state"]["series"][0]["lastX"], cycle);
        if (cycle % 2 == 0) {
            ASSERT_EQ(xf->QueueSetChildren(1, {}), (std::vector<int>{2, 3}));
            ASSERT_EQ(xf->QueueSetChildren(0, {}), (std::vector<int>{1}));
        } else {
            ASSERT_EQ(xf->QueueSetChildren(0, {}), (std::vector<int>{2, 3, 1}));
        }
        const auto state = xf->GetDiagnosticsState();
        ASSERT_EQ(state["elementCount"], baseline["elementCount"]) << cycle;
        ASSERT_EQ(state["hierarchyCount"], baseline["hierarchyCount"]) << cycle;
        ASSERT_EQ(state["internalSubjectCount"], baseline["internalSubjectCount"]) << cycle;
        ASSERT_TRUE(xf->QueueSetChildren(0, {}).empty());
    }
}

TEST_F(XFramesQueueTest, DelayedSubjectDeliveryCannotMutateAReusedNativeId) {
    Create({{"id", 2}, {"type", "plot-bar"}});
    xf->QueueSetChildren(1, {2});
    auto oldSubject = CopyInternalSubject(2);
    oldSubject.get_observer().on_next({{"op", "appendData"}, {"x", 1}, {"y", 2}});
    ASSERT_EQ(Node(xf->GetDiagnosticsState(), 2)["state"]["series"][0]["lastX"], 1);
    ASSERT_EQ(xf->QueueSetChildren(1, {}), (std::vector<int>{2}));
    Create({{"id", 2}, {"type", "plot-bar"}});
    xf->QueueSetChildren(1, {2});
    Internal(2, {{"op", "appendData"}, {"x", 10}, {"y", 20}});
    oldSubject.get_observer().on_next({{"op", "appendData"}, {"x", 99}, {"y", 99}});
    const auto state = xf->GetDiagnosticsState();
    EXPECT_EQ(Node(state, 2)["state"]["series"][0]["lastX"], 10);
    EXPECT_EQ(Node(state, 2)["state"]["series"][0]["count"], 1);
    EXPECT_EQ(state["internalSubjectCount"], 1);
}

TEST_F(XFramesQueueTest, DestructionResultsReportEachActualElementOnlyOnce) {
    Create({{"id", 2}, {"type", "plot-bar"}});
    xf->QueueSetChildren(1, {2});
    // Virtual roots have no Yoga owner, allowing this duplicate incoming list.
    xf->QueueSetChildren(0, {1, 1});
    EXPECT_EQ(xf->QueueSetChildren(0, {}), (std::vector<int>{2, 1}));
    EXPECT_TRUE(xf->QueueSetChildren(0, {}).empty());
    EXPECT_EQ(xf->GetDiagnosticsState()["elementCount"], 0);
}


namespace {
json Transaction(std::initializer_list<json> operations) {
    return {{"schemaVersion", 1}, {"surfaceId", 0}, {"operations", std::vector<json>(operations)}};
}
json TxCreate(int id, std::string type = "node", json props = json::object()) {
    return {{"op", "create"}, {"id", id}, {"elementType", type}, {"props", props}};
}
json TxChildren(int id, std::initializer_list<int> children) {
    return {{"op", "setChildren"}, {"parentId", id}, {"childrenIds", std::vector<int>(children)}};
}
json TxPatch(int id, json props) { return {{"op", "patch"}, {"id", id}, {"props", props}}; }
json TxAppend(int parent, int child) { return {{"op", "appendChild"}, {"parentId", parent}, {"childId", child}}; }
}

TEST_F(XFramesQueueTest, CommitSeveralOperationsShareOneRevisionAndPreserveWidgetData) {
    const auto result = xf->ApplyCommit(Transaction({TxCreate(2), TxCreate(3, "plot-bar"),
        TxCreate(4, "di-table", {{"columns", json::array({{{"fieldId", "v"}, {"heading", "Value"}}})}}),
        TxAppend(2, 3), TxAppend(2, 4), TxChildren(1, {2}),
        TxPatch(3, {{"series", json::array({{{"label", "Updated"}}})}})}).dump()).ToJson();
    ASSERT_EQ(result["status"], "applied") << result;
    EXPECT_EQ(result["nativeSequence"], "3"); EXPECT_EQ(result["nativeRevision"], "3");
    EXPECT_EQ(result["destroyedIds"], json::array());
    Internal(3, {{"op", "appendData"}, {"x", 42}, {"y", 7}});
    Internal(4, {{"op", "setData"}, {"data", json::array({{{"v", 19}}})}});
    auto state = xf->GetDiagnosticsState();
    const auto plot = Node(state, 3)["state"], table = Node(state, 4)["state"];
    EXPECT_EQ(plot["series"][0]["lastX"], 42); EXPECT_EQ(plot["series"][0]["label"], "Updated");
    EXPECT_EQ(table["rowCount"], 1);
    const auto reordered = xf->ApplyCommit(Transaction({TxChildren(2, {4, 3}), TxPatch(3, {{"showLegend", true}, {"series", json::array({{{"label", "Updated"}}})}})}).dump());
    ASSERT_EQ(reordered.status, "applied"); EXPECT_EQ(reordered.nativeRevision, 4);
    state = xf->GetDiagnosticsState();
    EXPECT_EQ(Node(state, 2)["yogaChildren"], json::array({4, 3}));
    EXPECT_EQ(Node(state, 3)["state"], plot); EXPECT_EQ(Node(state, 4)["state"], table);
    const auto removed = xf->ApplyCommit(Transaction({TxChildren(2, {4}), TxChildren(0, {})}).dump());
    EXPECT_EQ(removed.destroyedIds, (std::vector<int>{3, 4, 2, 1})); EXPECT_EQ(removed.nativeRevision, 5);
    state = xf->GetDiagnosticsState();
    EXPECT_EQ(state["elementCount"], 0); EXPECT_EQ(state["hierarchyCount"], 1); EXPECT_EQ(state["internalSubjectCount"], 0);
}

TEST_F(XFramesQueueTest, CommitInvalidFinalOperationsLeaveEveryNativeStateFieldUnchanged) {
    Create({{"id", 2}, {"type", "plot-bar"}}); xf->QueueSetChildren(1, {2});
    Internal(2, {{"op", "appendData"}, {"x", 10}, {"y", 20}});
    const auto before = xf->GetDiagnosticsState(), counters = xf->GetCommitState();
    const std::vector<std::pair<json, std::string>> cases = {
        {TxCreate(2), "duplicate_id"}, {TxPatch(999, json::object()), "missing_target"},
        {TxCreate(3, "unknown"), "invalid_element_type"}, {TxChildren(1, {2, 2}), "duplicate_child"},
        {TxChildren(1, {999}), "missing_target"}, {TxChildren(1, {1}), "cycle"},
        {TxPatch(2, {{"id", "public"}}), "immutable_identity"}, {TxPatch(2, {{"type", "node"}}), "immutable_identity"},
        {TxPatch(2, {{"axisAutoFit", "bad"}}), "invalid_props"},
        {TxPatch(2, {{"series", json::array({{{"label", 3}}})}}), "invalid_props"},
        {TxPatch(2, {{"style", {{"border", {{"thickness", "bad"}}}}}}), "invalid_props"},
        {TxCreate(3, "di-table", {{"columns", json::array({{{"heading", "missing fieldId"}}})}}), "invalid_props"},
        {{{"op", "future"}}, "unsupported_operation"}, {TxCreate(-1), "invalid_id"}
    };
    for (const auto& [invalid, code] : cases) {
        auto wire = Transaction({TxCreate(8, "plot-bar"), TxPatch(2, {{"showLegend", true}}), invalid});
        const auto result = xf->ApplyCommit(wire.dump()).ToJson(); SCOPED_TRACE(wire.dump());
        ASSERT_EQ(result["status"], "rejected") << result;
        EXPECT_EQ(result["error"]["code"], code); EXPECT_EQ(result["error"]["operationIndex"], 2);
        EXPECT_EQ(result["destroyedIds"], json::array()); EXPECT_TRUE(result["nativeSequence"].is_null());
        EXPECT_EQ(xf->GetDiagnosticsState().dump(), before.dump()); EXPECT_EQ(xf->GetCommitState(), counters);
    }
    auto rejected = xf->ApplyCommit(Transaction({TxChildren(1, {}), TxPatch(2, json::object())}).dump());
    EXPECT_EQ(rejected.error->code, "destroyed_id"); EXPECT_EQ(xf->GetDiagnosticsState().dump(), before.dump());
    const auto recovery = xf->ApplyCommit(Transaction({TxChildren(1, {})}).dump());
    EXPECT_EQ(recovery.status, "applied"); EXPECT_EQ(recovery.destroyedIds, (std::vector<int>{2}));
}

TEST_F(XFramesQueueTest, CommitRejectsEnvelopeAndIdErrorsAndRecovers) {
    const auto before = xf->GetDiagnosticsState();
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"{", "invalid_json"}, {"[]", "invalid_field"},
        {R"({"schemaVersion":2,"surfaceId":0,"operations":[]})", "unsupported_version"},
        {R"({"schemaVersion":1,"surfaceId":1,"operations":[]})", "unsupported_surface"},
        {R"({"schemaVersion":1,"surfaceId":"0","operations":[]})", "unsupported_surface"},
        {R"({"schemaVersion":1,"surfaceId":0})", "missing_field"},
        {R"({"schemaVersion":1,"surfaceId":0,"operations":{}})", "invalid_field"},
        {R"({"schemaVersion":1,"surfaceId":0,"operations":[],"sequence":1})", "unknown_field"}
    };
    for (const auto& [wire, code] : cases) {
        const auto result = xf->ApplyCommit(wire); SCOPED_TRACE(wire);
        ASSERT_TRUE(result.error.has_value()); EXPECT_EQ(result.status, "rejected"); EXPECT_EQ(result.error->code, code);
        EXPECT_EQ(result.nativeRevision, 2); EXPECT_EQ(xf->GetDiagnosticsState().dump(), before.dump());
    }
    for (const auto& id : json::array({0, -1, 1.5, 2147483648LL, 9007199254740993LL, "2", nullptr})) {
        auto op = TxCreate(2); op["id"] = id;
        const auto result = xf->ApplyCommit(Transaction({op}).dump());
        ASSERT_TRUE(result.error.has_value()); EXPECT_EQ(result.error->code, "invalid_id");
        EXPECT_EQ(xf->GetDiagnosticsState().dump(), before.dump());
    }
    EXPECT_EQ(xf->ApplyCommit(Transaction({TxCreate(2147483647)}).dump()).status, "applied");
}

TEST_F(XFramesQueueTest, CommitValidatesOrderedRelationshipsAndSameBatchIdReuse) {
    const auto before = xf->GetDiagnosticsState();
    const std::vector<std::pair<json, std::string>> cases = {
        {Transaction({TxAppend(1, 2), TxCreate(2)}), "missing_target"},
        {Transaction({TxCreate(2), TxCreate(2)}), "duplicate_id"},
        {Transaction({TxCreate(2), TxAppend(1, 2), TxChildren(1, {}), TxCreate(2)}), "destroyed_id"},
        {Transaction({TxCreate(2), TxAppend(1, 2), TxAppend(2, 1)}), "invalid_relationship"},
        {Transaction({TxCreate(2), TxCreate(3), TxAppend(1, 2), TxAppend(3, 2)}), "multiple_parents"},
        {Transaction({TxCreate(2, "unformatted-text", {{"text", "leaf"}}), TxCreate(3), TxAppend(2, 3)}), "invalid_relationship"},
        {Transaction({TxCreate(2), TxCreate(3), TxAppend(2, 3), TxAppend(3, 2)}), "cycle"}
    };
    for (const auto& [wire, code] : cases) {
        const auto result = xf->ApplyCommit(wire.dump()); SCOPED_TRACE(wire.dump());
        ASSERT_TRUE(result.error.has_value()) << result.ToJson(); EXPECT_EQ(result.error->code, code);
        EXPECT_EQ(xf->GetDiagnosticsState().dump(), before.dump());
    }
}

TEST_F(XFramesQueueTest, CommitEmptyNoOpsCompatibilityAndDiagnosticTogglesShareOrdering) {
    auto wire = Transaction({}); wire["correlationId"] = "same-correlation-is-not-ordering";
    EXPECT_EQ(xf->ApplyCommit(wire.dump()).nativeRevision, 3); EXPECT_EQ(xf->ApplyCommit(wire.dump()).nativeRevision, 4);
    Create({{"id", 2}, {"type", "node"}}); xf->QueueAppendChild(1, 2);
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], "6");
    EXPECT_EQ(xf->ApplyCommit(Transaction({TxAppend(1, 2), TxPatch(2, json::object())}).dump()).nativeRevision, 7);
    std::string props = "{}"; xf->QueuePatchElement(999, props);
    EXPECT_TRUE(xf->QueueSetChildren(999, {}).empty()); xf->QueueAppendChild(999, 2);
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], "10");
    xf->SetDiagnosticsEnabled(true); xf->SetUpSubjects(); xf->SetDiagnosticsEnabled(false);
    EXPECT_EQ(xf->GetCommitState()["nativeSequence"], "10");
    EXPECT_EQ(xf->ApplyCommit(Transaction({TxChildren(0, {})}).dump()).destroyedIds, (std::vector<int>{2, 1}));
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], "11");
}

TEST_F(XFramesTest, CommitCountersRemainLosslessAndOverflowRejectsBeforeMutation) {
    SetCommitCounters(9007199254740992ULL, 9007199254740992ULL);
    EXPECT_EQ(xf->ApplyCommit(Transaction({TxCreate(1)}).dump()).ToJson()["nativeRevision"], "9007199254740993");
    SetCommitCounters(UINT64_MAX, 12); const auto before = xf->GetDiagnosticsState();
    auto result = xf->ApplyCommit(Transaction({TxCreate(2)}).dump()); ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code, "counter_overflow"); EXPECT_EQ(xf->GetDiagnosticsState().dump(), before.dump());
    SetCommitCounters(12, UINT64_MAX); result = xf->ApplyCommit(Transaction({}).dump());
    EXPECT_EQ(result.error->code, "counter_overflow");
}

TEST_F(XFramesTest, CommitApplicationFailureIsHonestAndDoesNotTerminateSubject) {
    FailButtonCreation();
    const auto result = xf->ApplyCommit(Transaction({TxCreate(1), TxCreate(2, "di-button", {{"label", "Fail"}})}).dump());
    EXPECT_EQ(result.status, "failed"); ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code, "application_error"); EXPECT_EQ(result.error->operationIndex, 1);
    EXPECT_EQ(result.nativeSequence, 1); EXPECT_EQ(result.nativeRevision, 0);
    EXPECT_TRUE(xf->IsElementAlive(1)); // documented lack of arbitrary-failure rollback
    EXPECT_TRUE(LastCommitRequestExpired());
    EXPECT_EQ(xf->ApplyCommit(Transaction({TxCreate(3)}).dump()).nativeSequence, 2);
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], "1");
}


TEST_F(XFramesTest, CommitCompetingStructuralCallsHaveOneNativeAuthority) {
    std::vector<uint64_t> sequences;
    std::mutex resultsMutex;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread) threads.emplace_back([&, thread] {
        for (int i = 0; i < 30; ++i) {
            const int id = 1000 + thread * 100 + i;
            const auto result = xf->ApplyCommit(Transaction({TxCreate(id), TxAppend(0, id)}).dump());
            EXPECT_EQ(result.status, "applied");
            if (result.nativeSequence) {
                std::lock_guard<std::mutex> lock(resultsMutex);
                sequences.push_back(*result.nativeSequence);
            }
            // A stale legacy operation participates in the same serialization domain.
            std::string props = "{}";
            xf->QueuePatchElement(999999, props);
        }
    });
    for (auto& thread : threads) thread.join();
    EXPECT_EQ(sequences.size(), 120);
    std::sort(sequences.begin(), sequences.end());
    EXPECT_EQ(std::adjacent_find(sequences.begin(), sequences.end()), sequences.end());
    EXPECT_EQ(xf->GetCommitState()["nativeSequence"], "240");
    EXPECT_EQ(xf->GetCommitState()["nativeRevision"], "240");
    const auto removed = xf->ApplyCommit(Transaction({TxChildren(0, {})}).dump());
    EXPECT_EQ(removed.destroyedIds.size(), 120);
    EXPECT_EQ(xf->GetDiagnosticsState()["elementCount"], 1); // this fixture owns an actual node 0
}

TEST_F(XFramesQueueTest, CommitVirtualRootPartialRemovalPreservesTheSurvivingWidget) {
    Create({{"id", 2}, {"type", "node"}, {"root", true}});
    Create({{"id", 3}, {"type", "plot-bar"}});
    xf->QueueAppendChild(2, 3);
    xf->QueueSetChildren(0, {1, 2});
    Internal(3, {{"op", "appendData"}, {"x", 12}, {"y", 21}});
    const auto before = Node(xf->GetDiagnosticsState(), 3)["state"];
    const auto result = xf->ApplyCommit(Transaction({TxChildren(0, {2, 1}), TxChildren(0, {2})}).dump());
    EXPECT_EQ(result.status, "applied"); EXPECT_EQ(result.destroyedIds, (std::vector<int>{1}));
    const auto state = xf->GetDiagnosticsState();
    EXPECT_EQ(Node(state, 3)["state"], before);
    EXPECT_EQ(Node(state, 2)["yogaChildren"], json::array({3}));
    EXPECT_EQ(xf->ApplyCommit(Transaction({TxChildren(0, {})}).dump()).destroyedIds, (std::vector<int>{3, 2}));
}

TEST_F(XFramesQueueTest, CommitGuardedNullPropRemovalPreservesWidgetData) {
    ASSERT_EQ(xf->ApplyCommit(Transaction({
        TxCreate(2, "plot-bar", {{"series", json::array({{{"label", "Keep"}}})}}),
        TxCreate(3, "di-table", {{"columns", json::array({{{"fieldId", "v"}, {"heading", "Value"}}})}}),
        TxChildren(1, {2, 3})}).dump()).status, "applied");
    Internal(2, {{"op", "appendData"}, {"x", 12}, {"y", 21}});
    Internal(3, {{"op", "setData"}, {"data", json::array({{{"v", 19}}})}});
    const auto before = xf->GetDiagnosticsState();
    std::string plotPatch = R"({"series":null,"bullColor":null})";
    std::string tablePatch = R"({"columns":null,"contextMenuItems":null,"clipRows":null})";
    xf->QueuePatchElement(2, plotPatch);
    xf->QueuePatchElement(3, tablePatch);
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 2)["state"], Node(before, 2)["state"]);
    EXPECT_EQ(Node(xf->GetDiagnosticsState(), 3)["state"], Node(before, 3)["state"]);
    // Guarded scalar/style removals must remain no-ops at the same queue boundary.
    const auto removedOptions = xf->ApplyCommit(Transaction({
        TxCreate(4, "multi-slider", {{"numValues", nullptr}, {"decimalDigits", nullptr}, {"defaultValues", nullptr}}),
        TxCreate(5, "color-indicator", {{"color", nullptr}}),
        TxCreate(6, "di-window"),
        TxPatch(6, {{"title", nullptr}, {"width", nullptr}, {"height", nullptr}}),
        TxPatch(4, {{"style", {{"font", nullptr}, {"colors", nullptr}, {"vars", nullptr}, {"roundCorners", nullptr}}}}),
        TxChildren(1, {2, 3, 4, 5, 6})}).dump());
    EXPECT_EQ(removedOptions.status, "applied") << removedOptions.ToJson();
    EXPECT_EQ(xf->ApplyCommit(Transaction({TxChildren(1, {2, 3})}).dump()).destroyedIds, (std::vector<int>{4, 5, 6}));
}
