#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace xframes {

enum class FrameReason : uint8_t {
    Initial, Publication, Imperative, Input, Window, Resource, Screenshot,
    Style, Diagnostics, Layout, Cursor, KeyRepeat, Hover, Interaction, Canvas,
    Map, Retry, Count
};

// One runtime's ordering authority, independent of expensive tree diagnostics.
// Lock order: dispatch -> subject -> tree/resource visibility -> scheduler.
// The scheduler never acquires a tree lock or invokes an application observer.
class FrameScheduler {
public:
    using Time = std::chrono::steady_clock::time_point;
    using Clock = std::function<Time()>;
    using Wake = void (*)(void*) noexcept;
    static constexpr size_t MaxOwners = 4096;
    static constexpr size_t ReasonCount = static_cast<size_t>(FrameReason::Count);
    enum class Status { Running, Quarantined, Exhausted, Disposed, BackendFailed };

    struct Frame {
        uint64_t id = 0;
        uint64_t revision = 0;
        uint64_t generation = 0;
        uint32_t reasons = 0;
        Time capturedAt{};
        bool operator==(const Frame&) const = default;
    };
    struct Opportunity {
        bool render = false;
        std::optional<Time> deadline;
        bool terminal = false;
    };

private:
    struct State;
public:
    // Copy this endpoint into asynchronous completions. It never retains a
    // runtime/backend; an owner-scoped endpoint expires when that owner releases.
    class Source {
    public:
        Source() = default;
        uint64_t Invalidate(FrameReason reason) const;
        void Notify() const;
        void FailBackend() const;
        bool IsAlive() const;
    private:
        friend class FrameScheduler;
        Source(std::weak_ptr<State> state, uint64_t owner = 0) : m_state(std::move(state)), m_owner(owner) {}
        std::weak_ptr<State> m_state;
        uint64_t m_owner = 0;
    };
    // A registration belongs to a native lifetime, never to a reusable widget ID.
    // Late callbacks may hold a weak runtime and this token; release is idempotent.
    class Owner {
    public:
        Owner() = default;
        Owner(const Owner&) = delete;
        Owner& operator=(const Owner&) = delete;
        Owner(Owner&& other) noexcept;
        Owner& operator=(Owner&& other) noexcept;
        ~Owner();
        bool Set(bool active, std::optional<Time> deadline = std::nullopt);
        Source GetSource() const;
        void Reset();
    private:
        friend class FrameScheduler;
        Owner(std::weak_ptr<State> state, uint64_t id) : m_state(std::move(state)), m_id(id) {}
        std::weak_ptr<State> m_state;
        uint64_t m_id = 0;
    };

    explicit FrameScheduler(Clock clock = [] { return std::chrono::steady_clock::now(); });
    ~FrameScheduler();
    FrameScheduler(const FrameScheduler&) = delete;
    FrameScheduler& operator=(const FrameScheduler&) = delete;

    // Backend-only nonblocking notification (post event/request callback). It must
    // not call back into the scheduler, native tree, or JS application. Detach is
    // serialized with notification, and MUST precede backend/window destruction.
    void AttachWake(Wake wake, void* context);
    void DetachWake();

    // Publish mutation/queued work BEFORE this call, under the visibility lock
    // also used by Capture. Notify only after releasing the producer's locks.
    // Zero means terminal: it must never be reported as successful coverage.
    uint64_t Invalidate(FrameReason reason);
    bool CanInvalidate() const;
    Status GetStatus() const;
    uint64_t CoveredGeneration() const;
    void Notify();
    Owner Register(FrameReason reason);

    // Called after event draining. A non-render result ARMS waiting under the
    // same mutex as invalidation. The backend must enter its queued-event wait
    // without another event drain. Work racing that handoff posts one latched
    // notification; work preceding it is observed here. Deadlines use one clock.
    Opportunity TakeOpportunity();
    // Plan a future backend callback without counting it as an opportunity that
    // actually occurred. This also arms the paused browser loop for notifications.
    Opportunity PlanNext();
    Source GetSource() const;
    void DeferUntil(Time time);
    void SetRenderable(bool renderable);

    // Capture under native visibility locks BEFORE draw construction/resource
    // consumption. Work arriving after capture stays pending, even if rendering
    // happens to see some of it. Only an exact, successfully submitted ticket
    // can advance coverage. Skipped/failed submissions call Abandon.
    std::optional<Frame> Capture(uint64_t revision);
    bool Complete(const Frame& frame);
    bool Abandon(const Frame& frame);
    void Quarantine();
    void Dispose();
    nlohmann::json GetState() const;
    Time Now() const;

private:
    friend class FrameSchedulerTest;
    void SetCountersForTesting(uint64_t generation, uint64_t frameId);
    std::shared_ptr<State> m_state;
    Opportunity Plan(bool observed);
};
}
