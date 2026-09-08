#include "map_worker.h"
#ifndef __EMSCRIPTEN__
#include <algorithm>
#include <cstdio>

MapWorker::MapWorker() : m_state(std::make_shared<State>()) {
    try {
        for (size_t i = 0; i < WorkerCount; ++i) {
            std::thread([state = m_state] {
                std::unique_lock lock(state->mutex);
                ++state->threads;
                state->available.notify_all();
                for (;;) {
                    state->available.wait(lock, [&] { return state->stopped || !state->jobs.empty(); });
                    if (state->stopped) break;
                    auto job = std::move(state->jobs.front()); state->jobs.pop_front();
                    if (job.owner.expired()) continue;
                    ++state->active;
                    lock.unlock();
                    try { job.work(); }
                    catch (const std::exception& error) { fprintf(stderr, "Map worker failed: %s\n", error.what()); }
                    catch (...) { fprintf(stderr, "Map worker failed\n"); }
                    lock.lock();
                    --state->active;
                }
                --state->threads;
            }).detach();
        }
        std::unique_lock lock(m_state->mutex);
        m_state->available.wait(lock, [&] { return m_state->threads == WorkerCount; });
    } catch (...) { Stop(); throw; }
}

bool MapWorker::Submit(std::weak_ptr<void> owner, std::function<void()> work) {
    {
        const std::lock_guard lock(m_state->mutex);
        if (m_state->stopped || m_state->jobs.size() >= MaxQueued || owner.expired()) return false;
        m_state->jobs.push_back({std::move(owner), std::move(work)});
    }
    m_state->available.notify_one();
    return true;
}

void MapWorker::Cancel(const std::weak_ptr<void>& owner) {
    const std::lock_guard lock(m_state->mutex);
    std::erase_if(m_state->jobs, [&](const Job& job) {
        return !job.owner.owner_before(owner) && !owner.owner_before(job.owner);
    });
}

void MapWorker::Stop() {
    {
        const std::lock_guard lock(m_state->mutex);
        m_state->stopped = true;
        m_state->jobs.clear();
    }
    m_state->available.notify_all();
}

nlohmann::json MapWorker::Diagnostics() const {
    const std::lock_guard lock(m_state->mutex);
    return {{"queued", m_state->jobs.size()}, {"active", m_state->active}, {"threads", m_state->threads},
        {"stopped", m_state->stopped}, {"capacity", MaxQueued}};
}
#endif
