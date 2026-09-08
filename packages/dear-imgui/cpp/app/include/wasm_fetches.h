#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// The browser-thread request lifetime shared by Image, Map and Canvas loaders.
// Completion functions publish to widget-owned mailboxes, never reusable IDs.
// Closing an in-flight Emscripten 5.0.2 fetch calls onerror synchronously, so the
// cancellation guard must precede close and that callback must not close again.
class WasmFetches {
public:
    using Bytes = std::vector<unsigned char>;
    using Completion = std::function<void(bool, Bytes)>;
    WasmFetches() = default;
    WasmFetches(const WasmFetches&) = delete;
    WasmFetches& operator=(const WasmFetches&) = delete;
    ~WasmFetches() { CancelAll(); }
    size_t Size() const { return m_requests.size(); }

    void Cancel(const std::string& key) {
        for (auto it = m_requests.begin(); it != m_requests.end();) {
            if (it->second->key != key) { ++it; continue; }
            auto request = std::move(it->second);
            it = m_requests.erase(it);
            request->cancelled = true;
            emscripten_fetch_close(request->fetch);
        }
    }
    void CancelAll() {
        auto requests = std::move(m_requests);
        m_requests.clear();
        for (auto& [address, request] : requests) {
            request->cancelled = true;
            emscripten_fetch_close(request->fetch);
        }
    }
    void Get(std::string key, const std::string& url, Completion complete,
             std::unordered_map<std::string, std::string> headers = {}) {
        Cancel(key); // A replacement request supersedes only the same resource.
        auto request = std::make_unique<Request>();
        request->owner = this;
        request->key = std::move(key);
        request->complete = std::move(complete);
        request->headers = std::move(headers);
        for (const auto& [name, value] : request->headers) {
            request->headerPointers.push_back(name.c_str());
            request->headerPointers.push_back(value.c_str());
        }
        request->headerPointers.push_back(nullptr);
        auto* address = request.get();
        m_requests.emplace(address, std::move(request));
        emscripten_fetch_attr_t attributes;
        emscripten_fetch_attr_init(&attributes);
        std::strcpy(attributes.requestMethod, "GET");
        // Without REPLACE, Emscripten first reads IndexedDB asynchronously. Its
        // fetch has id=0 until that finishes, so fetch_close rejects cancellation
        // and a later callback can outlive this request. Our loaders own their
        // caches; start XHR immediately so every returned request is cancellable.
        attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_REPLACE;
        attributes.timeoutMSecs = 10'000;
        attributes.userData = address;
        attributes.requestHeaders = address->headerPointers.data();
        attributes.onsuccess = [](emscripten_fetch_t* fetch) { Finish(fetch, true); };
        attributes.onerror = [](emscripten_fetch_t* fetch) { Finish(fetch, false); };
        auto* fetch = emscripten_fetch(&attributes, url.c_str());
        // Defensive against a synchronous initiation failure invoking onerror.
        if (auto it = m_requests.find(address); it != m_requests.end()) {
            it->second->fetch = fetch;
            if (!fetch) {
                auto failed = std::move(it->second);
                m_requests.erase(it);
                failed->complete(false, {});
            }
        }
    }
private:
    struct Request {
        WasmFetches* owner = nullptr;
        std::string key;
        Completion complete;
        emscripten_fetch_t* fetch = nullptr;
        bool cancelled = false;
        std::unordered_map<std::string, std::string> headers;
        std::vector<const char*> headerPointers;
    };
    std::unordered_map<Request*, std::unique_ptr<Request>> m_requests;
    static void Finish(emscripten_fetch_t* fetch, bool success) {
        auto* address = static_cast<Request*>(fetch->userData);
        if (address->cancelled) return; // Cancel owns close and request destruction.
        auto node = address->owner->m_requests.extract(address);
        auto request = std::move(node.mapped());
        request->cancelled = true;
        struct CloseFetch {
            emscripten_fetch_t* fetch;
            ~CloseFetch() { if (fetch) emscripten_fetch_close(fetch); }
        } close{fetch};
        Bytes data;
        if (success && fetch->numBytes) data.assign(fetch->data, fetch->data + fetch->numBytes);
        emscripten_fetch_close(fetch);
        close.fetch = nullptr;
        request->complete(success, std::move(data));
    }
};
#endif
