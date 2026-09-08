#include <gtest/gtest.h>
#include <atomic>
#include <barrier>
#include <latch>
#include <limits>
#include <thread>
#include <vector>
#include "frame_scheduler.h"

namespace xframes {
using namespace std::chrono_literals;
class FrameSchedulerTest : public ::testing::Test {
protected:
    std::atomic<int64_t> milliseconds{0};
    FrameScheduler scheduler{[this] { return FrameScheduler::Time{} + std::chrono::milliseconds(milliseconds.load()); }};
    std::atomic<size_t> notifications{0};
    static void Wake(void* context) noexcept { ++*static_cast<std::atomic<size_t>*>(context); }
    void Attach() { scheduler.AttachWake(Wake, &notifications); }
    FrameScheduler::Frame Capture(uint64_t revision = 0) {
        auto frame = scheduler.Capture(revision);
        EXPECT_TRUE(frame);
        return frame.value();
    }
    void Submit(uint64_t revision = 0) { EXPECT_TRUE(scheduler.Complete(Capture(revision))); }
    void Settle() {
        ASSERT_TRUE(scheduler.TakeOpportunity().render);
        Submit();
        ASSERT_FALSE(scheduler.TakeOpportunity().render);
    }
    void Counters(uint64_t generation, uint64_t frameId) { scheduler.SetCountersForTesting(generation, frameId); }
};

TEST_F(FrameSchedulerTest, FirstFrameAndCoalescedPublicationsBecomeCleanOnlyOnSubmission) {
    Attach();
    EXPECT_EQ(notifications, 1);
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    EXPECT_EQ(scheduler.Invalidate(FrameReason::Publication), 2);
    EXPECT_EQ(scheduler.Invalidate(FrameReason::Publication), 3);
    scheduler.Notify();
    auto frame = Capture(2);
    EXPECT_EQ(frame.generation, 3);
    EXPECT_TRUE(scheduler.GetState()["dirty"]);
    EXPECT_TRUE(scheduler.Complete(frame));
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
    EXPECT_FALSE(scheduler.GetState()["dirty"]);
    EXPECT_EQ(scheduler.GetState()["completedFrame"]["nativeRevision"], "2");
    EXPECT_EQ(scheduler.GetState()["submitted"], "1");
}

TEST_F(FrameSchedulerTest, InvalidationBeforeSleepDecisionIsObservedWithoutNotification) {
    Attach();
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    Submit();
    std::latch published(1);
    std::thread producer([&] { scheduler.Invalidate(FrameReason::Resource); published.count_down(); });
    published.wait();
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    producer.join();
    EXPECT_EQ(notifications, 1);
}

TEST_F(FrameSchedulerTest, InvalidationBetweenWaitDecisionAndPlatformWaitLeavesLatchedWake) {
    Attach();
    Settle(); // Wait is now armed. No event drain is permitted before platform wait.
    std::latch armed(1), published(1);
    std::thread producer([&] {
        armed.wait();
        for (int i = 0; i < 100; ++i) { scheduler.Invalidate(FrameReason::Imperative); scheduler.Notify(); }
        published.count_down();
    });
    armed.count_down();
    published.wait();
    // The backend event remains queued even though it has not entered wait yet.
    EXPECT_EQ(notifications, 2);
    EXPECT_TRUE(scheduler.GetState()["notificationPending"]);
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    Submit();
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
    producer.join();
}

TEST_F(FrameSchedulerTest, InvalidationWhileWaitingIsSignalledAndRepeatedSleepRemainsSafe) {
    Attach();
    Settle();
    std::barrier rendezvous(2);
    std::thread producer([&] {
        for (int i = 0; i < 200; ++i) {
            rendezvous.arrive_and_wait();
            scheduler.Invalidate(FrameReason::Input);
            scheduler.Notify();
            rendezvous.arrive_and_wait();
        }
    });
    for (int i = 0; i < 200; ++i) {
        rendezvous.arrive_and_wait();
        rendezvous.arrive_and_wait();
        EXPECT_EQ(notifications, static_cast<size_t>(i + 2));
        EXPECT_TRUE(scheduler.TakeOpportunity().render);
        Submit();
        EXPECT_FALSE(scheduler.TakeOpportunity().render);
    }
    producer.join();
}

TEST_F(FrameSchedulerTest, WorkAfterCaptureCannotBeRelabelledAtCompletion) {
    scheduler.TakeOpportunity();
    auto frame = Capture(7);
    std::latch captured(1), applied(1);
    std::thread publisher([&] {
        captured.wait();
        scheduler.Invalidate(FrameReason::Publication);
        scheduler.Notify();
        applied.count_down();
    });
    captured.count_down();
    applied.wait();
    auto forged = frame;
    forged.generation = 2;
    forged.revision = 8;
    EXPECT_FALSE(scheduler.Complete(forged));
    EXPECT_TRUE(scheduler.Complete(frame));
    auto state = scheduler.GetState();
    EXPECT_EQ(state["completedFrame"]["nativeRevision"], "7");
    EXPECT_EQ(state["coveredGeneration"], "1");
    EXPECT_TRUE(state["dirty"]);
    EXPECT_TRUE(state["reasons"]["publication"]["pending"]);
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    Submit(8);
    EXPECT_FALSE(scheduler.GetState()["dirty"]);
    publisher.join();
}

TEST_F(FrameSchedulerTest, CompletionAndInvalidationRaceCannotClearNewWork) {
    for (int i = 0; i < 100; ++i) {
        auto frame = Capture();
        std::barrier start(2);
        std::thread producer([&] { start.arrive_and_wait(); scheduler.Invalidate(FrameReason::Input); scheduler.Notify(); });
        start.arrive_and_wait();
        EXPECT_TRUE(scheduler.Complete(frame));
        producer.join();
        EXPECT_TRUE(scheduler.TakeOpportunity().render);
        EXPECT_TRUE(scheduler.GetState()["dirty"]);
    }
}

TEST_F(FrameSchedulerTest, DeadlineUsesControlledMonotonicClockAndCancellationWakesWaiter) {
    Attach(); Settle();
    auto late = scheduler.Register(FrameReason::Hover);
    auto early = scheduler.Register(FrameReason::Map);
    late.Set(false, scheduler.Now() + 150ms);
    early.Set(false, scheduler.Now() + 50ms);
    auto opportunity = scheduler.TakeOpportunity();
    EXPECT_FALSE(opportunity.render);
    EXPECT_EQ(opportunity.deadline, scheduler.Now() + 50ms);
    const auto before = notifications.load();
    early.Reset();
    EXPECT_EQ(notifications, before + 1);
    EXPECT_EQ(scheduler.TakeOpportunity().deadline, scheduler.Now() + 150ms);
    milliseconds = 149;
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
    milliseconds = 150;
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    EXPECT_EQ(scheduler.GetState()["reasons"]["hover"]["deadlinesFired"], "1");
    Submit();
    EXPECT_FALSE(scheduler.TakeOpportunity().render); // Deadline is one shot.
    EXPECT_EQ(scheduler.GetState()["deadlines"], 0);
}

TEST_F(FrameSchedulerTest, ActiveOwnerUsesEveryBackendOpportunityAndReleasesOnMoveOrDestruction) {
    Settle();
    {
        auto owner = scheduler.Register(FrameReason::Canvas);
        owner.Set(true);
        for (int i = 0; i < 120; ++i) {
            milliseconds = i * 8; // No 33 ms frame cap in the scheduler.
            ASSERT_TRUE(scheduler.TakeOpportunity().render);
            Submit();
        }
        auto moved = std::move(owner);
        owner.Reset();
        EXPECT_EQ(scheduler.GetState()["activeOwners"], 1);
    }
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
    EXPECT_EQ(scheduler.GetState()["ownerCount"], 0);
}

TEST_F(FrameSchedulerTest, HiddenSurfaceRetainsDirtyStateAndExpiredDeadlineUntilRestored) {
    Settle();
    auto owner = scheduler.Register(FrameReason::Map);
    owner.Set(false, scheduler.Now() + 100ms);
    scheduler.SetRenderable(false);
    scheduler.Invalidate(FrameReason::Resource);
    milliseconds = 200;
    auto opportunity = scheduler.TakeOpportunity();
    EXPECT_FALSE(opportunity.render);
    EXPECT_FALSE(opportunity.deadline);
    EXPECT_FALSE(scheduler.Capture(1));
    EXPECT_TRUE(scheduler.GetState()["dirty"]);
    scheduler.SetRenderable(true);
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    Submit();
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
}

TEST_F(FrameSchedulerTest, SkippedSubmissionAndDuplicateCompletionCannotAdvanceCoverage) {
    auto frame = Capture();
    EXPECT_THROW(scheduler.Capture(0), std::logic_error);
    EXPECT_TRUE(scheduler.Abandon(frame));
    EXPECT_FALSE(scheduler.Complete(frame));
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    auto next = Capture();
    EXPECT_TRUE(scheduler.Complete(next));
    EXPECT_FALSE(scheduler.Complete(next));
    EXPECT_EQ(scheduler.GetState()["submitted"], "1");
    EXPECT_EQ(scheduler.GetState()["abandoned"], "1");
}

TEST_F(FrameSchedulerTest, CountersAreLosslessAndExhaustionIsTerminalWithoutFalseCompletion) {
    const auto max = std::numeric_limits<uint64_t>::max();
    Counters(max - 1, max - 1);
    EXPECT_EQ(scheduler.Invalidate(FrameReason::Publication), max);
    auto frame = Capture(max);
    EXPECT_TRUE(scheduler.Complete(frame));
    EXPECT_EQ(scheduler.GetState()["completedFrame"]["frameId"], "18446744073709551615");
    EXPECT_EQ(scheduler.GetState()["coveredGeneration"], "18446744073709551615");
    EXPECT_FALSE(scheduler.CanInvalidate());
    EXPECT_EQ(scheduler.Invalidate(FrameReason::Input), 0);
    EXPECT_EQ(scheduler.GetState()["status"], "exhausted");
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
    EXPECT_FALSE(scheduler.Capture(max));
}
TEST_F(FrameSchedulerTest, FrameCounterExhaustionDoesNotWrapOrClaimSubmission) {
    Counters(1, std::numeric_limits<uint64_t>::max());
    EXPECT_FALSE(scheduler.Capture(0));
    EXPECT_EQ(scheduler.GetState()["status"], "exhausted");
    EXPECT_EQ(scheduler.GetState()["coveredGeneration"], "0");
}

TEST_F(FrameSchedulerTest, QuarantineWakesOnceAndRejectsPendingFrameAndActivity) {
    Attach(); Settle();
    auto owner = scheduler.Register(FrameReason::Canvas);
    owner.Set(true);
    auto frame = Capture();
    scheduler.Quarantine();
    scheduler.Notify();
    scheduler.Notify();
    EXPECT_EQ(notifications, 2);
    EXPECT_FALSE(scheduler.Complete(frame));
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
    EXPECT_FALSE(owner.Set(true));
    EXPECT_EQ(scheduler.GetState()["ownerCount"], 0);
    EXPECT_EQ(scheduler.Invalidate(FrameReason::Publication), 0);
}

TEST_F(FrameSchedulerTest, OwnerAndCorrelationStorageAreBoundedAndReuseNeverRevivesOldOwner) {
    std::vector<FrameScheduler::Owner> owners;
    for (size_t i = 0; i < FrameScheduler::MaxOwners; ++i) owners.push_back(scheduler.Register(FrameReason::Canvas));
    EXPECT_THROW(scheduler.Register(FrameReason::Map), std::length_error);
    owners.front().Reset();
    auto replacement = scheduler.Register(FrameReason::Map);
    replacement.Set(true);
    EXPECT_FALSE(owners.front().Set(true));
    EXPECT_EQ(scheduler.GetState()["activeOwners"], 1);
    for (int i = 0; i < 100; ++i) {
        auto frame = Capture();
        EXPECT_LE(scheduler.GetState()["correlationRecords"], 2);
        EXPECT_TRUE(scheduler.Complete(frame));
    }
    owners.clear(); replacement.Reset();
    EXPECT_EQ(scheduler.GetState()["ownerCount"], 0);
    EXPECT_EQ(scheduler.GetState()["correlationRecords"], 1);
}

TEST_F(FrameSchedulerTest, DetachAndDisposalPreventLateCallbacksAndKeepOrdering) {
    Attach(); Settle();
    auto owner = scheduler.Register(FrameReason::Canvas);
    scheduler.DetachWake();
    scheduler.Invalidate(FrameReason::Resource);
    scheduler.Notify();
    EXPECT_EQ(notifications, 1);
    Attach();
    EXPECT_EQ(notifications, 2);
    const auto before = scheduler.GetState()["generation"];
    scheduler.Dispose();
    scheduler.Notify();
    EXPECT_FALSE(owner.Set(true));
    EXPECT_EQ(scheduler.Invalidate(FrameReason::Input), 0);
    EXPECT_EQ(scheduler.GetState()["generation"], before);
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
    EXPECT_EQ(notifications, 2);
    EXPECT_EQ(scheduler.GetState()["ownerCount"], 0);
    EXPECT_FALSE(scheduler.GetState()["wakeAttached"]);
}

TEST_F(FrameSchedulerTest, BrowserPlanningDoesNotInventFrameOpportunities) {
    Attach();
    for (int i = 0; i < 100; ++i) EXPECT_TRUE(scheduler.PlanNext().render);
    EXPECT_EQ(scheduler.GetState()["opportunities"], "0");
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    Submit();
    for (int i = 0; i < 100; ++i) EXPECT_FALSE(scheduler.PlanNext().render);
    EXPECT_EQ(scheduler.GetState()["opportunities"], "1");
    EXPECT_EQ(scheduler.GetState()["skippedOpportunities"], "0");
    scheduler.Invalidate(FrameReason::Publication);
    scheduler.Notify();
    EXPECT_EQ(notifications, 2);
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
}

TEST_F(FrameSchedulerTest, RecoverableRetryRetainsCoverageAndWaitsUntilExplicitDeadline) {
    Attach();
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    auto failed = Capture(9);
    EXPECT_TRUE(scheduler.Abandon(failed));
    scheduler.DeferUntil(scheduler.Now() + 64ms);
    auto next = scheduler.PlanNext();
    EXPECT_FALSE(next.render);
    EXPECT_EQ(next.deadline, scheduler.Now() + 64ms);
    scheduler.Invalidate(FrameReason::Publication);
    scheduler.Notify();
    milliseconds = 63;
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
    EXPECT_EQ(scheduler.CoveredGeneration(), 0);
    milliseconds = 64;
    EXPECT_TRUE(scheduler.TakeOpportunity().render);
    Submit(10);
    EXPECT_FALSE(scheduler.PlanNext().render);
    EXPECT_EQ(scheduler.CoveredGeneration(), 2);
}

TEST_F(FrameSchedulerTest, AsyncSourceExpiresWithOwnerEvenWhenNativeIdIsReused) {
    Attach(); Settle();
    auto owner = scheduler.Register(FrameReason::Resource);
    const auto old = owner.GetSource();
    EXPECT_EQ(old.Invalidate(FrameReason::Resource), 2);
    old.Notify();
    owner.Reset();
    auto replacement = scheduler.Register(FrameReason::Resource);
    Submit();
    EXPECT_FALSE(scheduler.TakeOpportunity().render);
    const auto before = scheduler.GetState();
    EXPECT_EQ(old.Invalidate(FrameReason::Resource), 0);
    old.Notify(); old.FailBackend();
    EXPECT_EQ(scheduler.GetState(), before);
    EXPECT_EQ(replacement.GetSource().Invalidate(FrameReason::Resource), 3);
}

TEST_F(FrameSchedulerTest, DeviceFailureIsTerminalWithoutFalselyCompletingDrawOrRetainingBackend) {
    Attach(); Settle();
    const auto source = scheduler.GetSource();
    auto frame = Capture(12);
    source.FailBackend();
    EXPECT_TRUE(scheduler.PlanNext().terminal);
    EXPECT_FALSE(scheduler.PlanNext().render);
    EXPECT_FALSE(scheduler.Complete(frame));
    EXPECT_EQ(scheduler.GetStatus(), FrameScheduler::Status::BackendFailed);
    EXPECT_EQ(scheduler.GetState()["completedFrame"]["nativeRevision"], "0");
    EXPECT_EQ(notifications, 2);
    scheduler.DetachWake(); scheduler.Dispose();
    source.Notify(); source.FailBackend();
    EXPECT_EQ(notifications, 2);
    EXPECT_EQ(scheduler.GetStatus(), FrameScheduler::Status::Disposed);
}
}
