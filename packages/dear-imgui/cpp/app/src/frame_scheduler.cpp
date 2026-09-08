#include "frame_scheduler.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace xframes {
namespace {
constexpr std::array<const char*, FrameScheduler::ReasonCount> reasonNames = {
    "initial", "publication", "imperative", "input", "window", "resource",
    "screenshot", "style", "diagnostics", "layout", "cursor", "keyRepeat",
    "hover", "interaction", "canvas", "map", "retry"
};
size_t Index(FrameReason reason) {
    const auto index = static_cast<size_t>(reason);
    if (index >= reasonNames.size()) throw std::invalid_argument("Unsupported frame reason");
    return index;
}
double Milliseconds(FrameScheduler::Time time) {
    return std::chrono::duration<double, std::milli>(time.time_since_epoch()).count();
}
}

struct FrameScheduler::State {
    explicit State(Clock clock) : clock(std::move(clock)) {
        reasonGenerations[0] = 1;
        invalidations[0] = 1;
    }
    mutable std::mutex mutex;
    Clock clock;
    Status status = Status::Running;
    bool renderable = true;
    bool waiting = false;
    bool notificationPending = false;
    Wake wake = nullptr;
    void* wakeContext = nullptr;
    uint64_t generation = 1; // First frame, including an empty ready runtime.
    uint64_t covered = 0;
    uint64_t nextFrame = 0;
    uint64_t nextOwner = 0;
    std::optional<Frame> constructing;
    std::optional<Frame> completed;
    Time submittedAt{};
    Time lastInvalidatedAt{};
    Time lastWakeAt{};
    std::optional<Time> deferredUntil;
    std::array<uint64_t, ReasonCount> reasonGenerations{};
    std::array<uint64_t, ReasonCount> invalidations{};
    std::array<uint64_t, ReasonCount> deadlinesFired{};
    struct Activity { FrameReason reason; bool active = false; std::optional<Time> deadline; };
    std::unordered_map<uint64_t, Activity> owners;
    uint64_t wakeCount = 0, opportunities = 0, skippedOpportunities = 0;
    uint64_t constructed = 0, submitted = 0, abandoned = 0;
    uint64_t ownerHighWater = 0, rejectedOwners = 0, metricOverflows = 0;

    void Count(uint64_t& counter) {
        if (counter != std::numeric_limits<uint64_t>::max()) ++counter;
        else if (metricOverflows != std::numeric_limits<uint64_t>::max()) ++metricOverflows;
    }
    void Signal() {
        if (wake && waiting && !notificationPending && status != Status::Disposed) {
            notificationPending = true;
            Count(wakeCount);
            lastWakeAt = clock();
            wake(wakeContext);
        }
    }
    void Terminal(Status value) {
        status = value;
        owners.clear();
        constructing.reset();
        Signal();
    }
    uint64_t Invalidate(size_t reason) {
        if (status != Status::Running) return 0;
        if (generation == std::numeric_limits<uint64_t>::max()) {
            Terminal(Status::Exhausted);
            return 0;
        }
        ++generation;
        reasonGenerations[reason] = generation;
        Count(invalidations[reason]);
        lastInvalidatedAt = clock();
        return generation;
    }
    std::optional<Time> Deadline() const {
        std::optional<Time> next;
        for (const auto& [id, activity] : owners)
            if (activity.deadline && (!next || *activity.deadline < *next)) next = activity.deadline;
        return next;
    }
};

FrameScheduler::FrameScheduler(Clock clock) : m_state(std::make_shared<State>(std::move(clock))) {}
FrameScheduler::~FrameScheduler() { Dispose(); }
FrameScheduler::Time FrameScheduler::Now() const { return m_state->clock(); }
FrameScheduler::Source FrameScheduler::GetSource() const { return Source(m_state); }
FrameScheduler::Source FrameScheduler::Owner::GetSource() const { return Source(m_state, m_id); }
uint64_t FrameScheduler::Source::Invalidate(FrameReason reason) const {
    auto state = m_state.lock();
    if (!state) return 0;
    const auto index = Index(reason);
    const std::lock_guard lock(state->mutex);
    if (m_owner && !state->owners.contains(m_owner)) return 0;
    return state->Invalidate(index);
}
void FrameScheduler::Source::Notify() const {
    if (auto state = m_state.lock()) {
        const std::lock_guard lock(state->mutex);
        if (!m_owner || state->owners.contains(m_owner)) state->Signal();
    }
}
void FrameScheduler::Source::FailBackend() const {
    if (auto state = m_state.lock()) {
        const std::lock_guard lock(state->mutex);
        if ((!m_owner || state->owners.contains(m_owner)) && state->status == Status::Running)
            state->Terminal(Status::BackendFailed);
    }
}
bool FrameScheduler::Source::IsAlive() const {
    if (auto state = m_state.lock()) {
        const std::lock_guard lock(state->mutex);
        return state->status == Status::Running && (!m_owner || state->owners.contains(m_owner));
    }
    return false;
}

void FrameScheduler::AttachWake(Wake wake, void* context) {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    if (s.status == Status::Disposed) return;
    if (s.wake) throw std::logic_error("A runtime supports one scheduler wake owner");
    s.wake = wake;
    s.wakeContext = context;
    s.notificationPending = false;
    s.waiting = true;
    s.Signal(); // Schedule the initial opportunity, including existing pending work.
}
void FrameScheduler::DetachWake() {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    s.wake = nullptr;
    s.wakeContext = nullptr;
    s.notificationPending = false;
    s.waiting = false;
}
uint64_t FrameScheduler::Invalidate(FrameReason reason) {
    auto& s = *m_state;
    const auto index = Index(reason);
    const std::lock_guard lock(s.mutex);
    return s.Invalidate(index);
}
bool FrameScheduler::CanInvalidate() const {
    const std::lock_guard lock(m_state->mutex);
    return m_state->status == Status::Running && m_state->generation != std::numeric_limits<uint64_t>::max();
}
FrameScheduler::Status FrameScheduler::GetStatus() const {
    const std::lock_guard lock(m_state->mutex);
    return m_state->status;
}
uint64_t FrameScheduler::CoveredGeneration() const {
    const std::lock_guard lock(m_state->mutex);
    return m_state->covered;
}
void FrameScheduler::SetCountersForTesting(uint64_t generation, uint64_t frameId) {
    const std::lock_guard lock(m_state->mutex);
    m_state->generation = generation;
    m_state->nextFrame = frameId;
}
void FrameScheduler::Notify() {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    s.Signal();
}

FrameScheduler::Owner FrameScheduler::Register(FrameReason reason) {
    Index(reason);
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    if (s.status != Status::Running) return {};
    if (s.owners.size() == MaxOwners || s.nextOwner == std::numeric_limits<uint64_t>::max()) {
        s.Count(s.rejectedOwners);
        throw std::length_error("Frame activity owner limit exhausted");
    }
    const auto id = ++s.nextOwner;
    s.owners.emplace(id, State::Activity{reason});
    s.ownerHighWater = std::max(s.ownerHighWater, static_cast<uint64_t>(s.owners.size()));
    return Owner(m_state, id);
}
FrameScheduler::Owner::Owner(Owner&& other) noexcept
    : m_state(std::move(other.m_state)), m_id(std::exchange(other.m_id, 0)) {}
FrameScheduler::Owner& FrameScheduler::Owner::operator=(Owner&& other) noexcept {
    if (this != &other) {
        Reset();
        m_state = std::move(other.m_state);
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}
FrameScheduler::Owner::~Owner() { Reset(); }
bool FrameScheduler::Owner::Set(bool active, std::optional<Time> deadline) {
    auto state = m_state.lock();
    if (!state) return false;
    const std::lock_guard lock(state->mutex);
    auto it = state->owners.find(m_id);
    if (state->status != Status::Running || it == state->owners.end()) return false;
    auto& activity = it->second;
    if (activity.active != active || activity.deadline != deadline) {
        activity.active = active;
        activity.deadline = deadline;
        state->Signal(); // Re-evaluate a sleeping backend's timer, also on cancellation.
    }
    return true;
}
void FrameScheduler::Owner::Reset() {
    if (auto state = m_state.lock()) {
        const std::lock_guard lock(state->mutex);
        if (auto it = state->owners.find(m_id); it != state->owners.end()) {
            const bool scheduled = it->second.active || it->second.deadline.has_value();
            state->owners.erase(it);
            if (scheduled) state->Signal();
        }
    }
    m_id = 0;
    m_state.reset();
}

FrameScheduler::Opportunity FrameScheduler::TakeOpportunity() { return Plan(true); }
FrameScheduler::Opportunity FrameScheduler::PlanNext() { return Plan(false); }
FrameScheduler::Opportunity FrameScheduler::Plan(bool observed) {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    if (observed) {
        s.Count(s.opportunities);
        s.notificationPending = false;
    }
    s.waiting = false;
    Opportunity result;
    result.terminal = s.status != Status::Running;
    if (s.status == Status::Running && s.renderable) {
        const auto now = s.clock();
        std::array<bool, ReasonCount> due{};
        for (auto& [id, activity] : s.owners) {
            if (activity.deadline && *activity.deadline <= now) {
                activity.deadline.reset(); // One shot; the consumer may rearm explicitly.
                due[Index(activity.reason)] = true;
            }
            result.render |= activity.active;
        }
        for (size_t i = 0; i < due.size(); ++i) if (due[i]) {
            s.Count(s.deadlinesFired[i]);
            s.Invalidate(i);
        }
        result.render |= s.generation > s.covered;
        result.render &= s.status == Status::Running;
        result.deadline = s.Deadline();
        if (s.deferredUntil) {
            if (*s.deferredUntil > now) {
                result.render = false;
                result.deadline = s.deferredUntil;
            } else s.deferredUntil.reset();
        }
        result.terminal = s.status != Status::Running;
    }
    if (!result.render) {
        s.waiting = s.status != Status::Disposed;
        if (observed) s.Count(s.skippedOpportunities);
    }
    return result;
}
void FrameScheduler::DeferUntil(Time time) {
    const std::lock_guard lock(m_state->mutex);
    m_state->deferredUntil = time;
}

void FrameScheduler::SetRenderable(bool renderable) {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    if (s.renderable == renderable || s.status != Status::Running) return;
    s.renderable = renderable;
    if (renderable) s.Invalidate(Index(FrameReason::Window));
    s.Signal();
}

std::optional<FrameScheduler::Frame> FrameScheduler::Capture(uint64_t revision) {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    if (s.status != Status::Running || !s.renderable) return std::nullopt;
    if (s.constructing) throw std::logic_error("Unsubmitted frame must be abandoned before constructing another");
    if (s.nextFrame == std::numeric_limits<uint64_t>::max()) {
        s.Terminal(Status::Exhausted);
        return std::nullopt;
    }
    uint32_t reasons = 0;
    for (size_t i = 0; i < ReasonCount; ++i)
        if (s.reasonGenerations[i] > s.covered) reasons |= uint32_t{1} << i;
    for (const auto& [id, activity] : s.owners)
        if (activity.active) reasons |= uint32_t{1} << Index(activity.reason);
    s.constructing = Frame{++s.nextFrame, revision, s.generation, reasons, s.clock()};
    s.Count(s.constructed);
    return s.constructing;
}
bool FrameScheduler::Complete(const Frame& frame) {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    if (s.status != Status::Running || !s.constructing || *s.constructing != frame) return false;
    s.covered = frame.generation; // Never read the latest generation/revision here.
    s.completed = frame;
    s.submittedAt = s.clock();
    s.constructing.reset();
    s.Count(s.submitted);
    return true;
}
bool FrameScheduler::Abandon(const Frame& frame) {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    if (!s.constructing || *s.constructing != frame) return false;
    s.constructing.reset();
    s.Count(s.abandoned);
    return true;
}
void FrameScheduler::Quarantine() {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    if (s.status == Status::Running) s.Terminal(Status::Quarantined);
}
void FrameScheduler::Dispose() {
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    // Backend stops/joins its loop before disposal. No wake can outlive detach.
    s.status = Status::Disposed;
    s.wake = nullptr;
    s.wakeContext = nullptr;
    s.waiting = s.notificationPending = false;
    s.constructing.reset();
    s.owners.clear();
}

nlohmann::json FrameScheduler::GetState() const {
    using json = nlohmann::json;
    auto& s = *m_state;
    const std::lock_guard lock(s.mutex);
    const char* statuses[] = {"running", "quarantined", "exhausted", "disposed", "backendFailed"};
    json reasons = json::object();
    size_t activeOwners = 0, deadlines = 0;
    std::array<size_t, ReasonCount> active{}, timed{};
    for (const auto& [id, activity] : s.owners) {
        if (activity.active) { ++activeOwners; ++active[Index(activity.reason)]; }
        if (activity.deadline) { ++deadlines; ++timed[Index(activity.reason)]; }
    }
    for (size_t i = 0; i < ReasonCount; ++i) reasons[reasonNames[i]] = {
        {"invalidations", std::to_string(s.invalidations[i])},
        {"pending", s.reasonGenerations[i] > s.covered}, {"activeOwners", active[i]},
        {"deadlines", timed[i]}, {"deadlinesFired", std::to_string(s.deadlinesFired[i])}
    };
    const auto deadline = s.Deadline();
    json completed = nullptr;
    if (s.completed) completed = {
        {"frameId", std::to_string(s.completed->id)},
        {"nativeRevision", std::to_string(s.completed->revision)},
        {"coveredGeneration", std::to_string(s.completed->generation)},
        {"reasons", s.completed->reasons}, {"capturedAtMs", Milliseconds(s.completed->capturedAt)},
        {"submittedAtMs", Milliseconds(s.submittedAt)}
    };
    return {
        {"status", statuses[static_cast<size_t>(s.status)]}, {"renderable", s.renderable},
        {"generation", std::to_string(s.generation)}, {"coveredGeneration", std::to_string(s.covered)},
        {"dirty", s.generation > s.covered}, {"waiting", s.waiting},
        {"notificationPending", s.notificationPending}, {"wakeAttached", s.wake != nullptr},
        {"ownerCount", s.owners.size()}, {"activeOwners", activeOwners}, {"deadlines", deadlines},
        {"nextDeadlineMs", deadline ? json(Milliseconds(*deadline)) : json(nullptr)},
        {"deferredUntilMs", s.deferredUntil ? json(Milliseconds(*s.deferredUntil)) : json(nullptr)},
        {"ownerHighWater", std::to_string(s.ownerHighWater)}, {"rejectedOwners", std::to_string(s.rejectedOwners)},
        {"wakeCount", std::to_string(s.wakeCount)}, {"opportunities", std::to_string(s.opportunities)},
        {"skippedOpportunities", std::to_string(s.skippedOpportunities)},
        {"constructed", std::to_string(s.constructed)}, {"submitted", std::to_string(s.submitted)},
        {"abandoned", std::to_string(s.abandoned)}, {"metricOverflows", std::to_string(s.metricOverflows)},
        {"lastInvalidatedAtMs", Milliseconds(s.lastInvalidatedAt)}, {"lastWakeAtMs", Milliseconds(s.lastWakeAt)},
        {"correlationRecords", (s.constructing ? 1 : 0) + (s.completed ? 1 : 0)},
        {"completedFrame", std::move(completed)}, {"reasons", std::move(reasons)}
    };
}
}
