#pragma once

#ifndef __EMSCRIPTEN__
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

// A runtime's bounded Map download workers. Tasks retain weak resource mailboxes,
// never widgets/runtimes. Cancellation removes queued tasks. At most four active
// HTTP calls may finish after cancellation, bounded by fetchTile's 10-s timeout.
class MapWorker {
public:
    static constexpr size_t MaxQueued = 2048, WorkerCount = 4;
    MapWorker();
    ~MapWorker() { Stop(); }
    bool Submit(std::weak_ptr<void> owner, std::function<void()> work);
    void Cancel(const std::weak_ptr<void>& owner);
    void Stop();
    nlohmann::json Diagnostics() const;
private:
    struct Job { std::weak_ptr<void> owner; std::function<void()> work; };
    struct State {
        std::mutex mutex;
        std::condition_variable available;
        std::deque<Job> jobs;
        bool stopped = false;
        size_t active = 0, threads = 0;
    };
    std::shared_ptr<State> m_state;
};
#endif
