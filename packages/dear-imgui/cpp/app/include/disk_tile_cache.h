#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct TileCacheStats {
    int memoryHits = 0;
    int diskHits = 0;
    int networkFetches = 0;
};

class DiskTileCache {
public:
    void configure(const std::string& basePath);
    bool isEnabled() const;
    std::optional<std::vector<uint8_t>> get(int x, int y, int zoom);
    void put(int x, int y, int zoom, const void* data, size_t numBytes);

private:
    std::mutex m_mutex;
    std::filesystem::path m_basePath;
    bool m_enabled = false;

    std::filesystem::path tilePath(int x, int y, int zoom) const;
};
