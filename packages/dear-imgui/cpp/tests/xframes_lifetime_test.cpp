// Element lifetimes on real frames: subjects, imperative data, asynchronous jobs, diagnostics and disposal.
#include "xframes_test_support.h"

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
