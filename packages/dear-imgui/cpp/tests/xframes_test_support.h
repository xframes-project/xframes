// Fixtures and helpers shared by the XFrames engine tests.
#pragma once

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
#include "widget/tree_node.h"
#include "widget/checkbox.h"
#include "widget/combo.h"
#include "widget/text.h"

using json = nlohmann::json;
inline json Create(int id, std::string type = "node", json props = json::object()) {
    return {{"op", "create"}, {"id", id}, {"elementType", type}, {"props", props}};
}
inline json Patch(int id, json props) { return {{"op", "patch"}, {"id", id}, {"props", props}}; }
inline json Children(int id, std::vector<int> children) {
    return {{"op", "setChildren"}, {"parentId", id}, {"childrenIds", children}};
}
inline json TableProps() { return {{"columns", json::array({{{"fieldId", "v"}, {"heading", "Value"}, {"type", "number"}}})}}; }

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
