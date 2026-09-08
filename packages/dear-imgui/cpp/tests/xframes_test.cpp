#include <gtest/gtest.h>
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
class XFramesQueueTest : public ::testing::Test {
protected:
    std::unique_ptr<XFrames> xf;
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

TEST_F(XFramesQueueTest, CharacterizeContainerUnmountAndSameIdReparentDefects) {
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
    const bool unmountInvariant = state["elementCount"] == 0;
    EXPECT_FALSE(unmountInvariant) << "XPASS XF-LIFE-010: remove the expected failure after container cleanup is fixed";
    EXPECT_EQ(state["elementCount"], 3) << "XF-LIFE-010 signature changed";
    EXPECT_EQ(state["unreachableCount"], 3);
    RecordProperty("known_defects", "XF-LIFE-005,XF-LIFE-010");
}
