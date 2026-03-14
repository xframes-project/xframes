#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <rpp/rpp.hpp>
#include "widget/styled_widget.h"
#include "xframes.h"
#include "element/element.h"

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
