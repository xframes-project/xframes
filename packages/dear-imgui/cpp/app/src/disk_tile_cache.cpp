#include "disk_tile_cache.h"

void DiskTileCache::configure(const std::string& basePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_basePath = basePath;
    m_enabled = true;

    std::error_code ec;
    std::filesystem::create_directories(m_basePath, ec);
}

bool DiskTileCache::isEnabled() const {
    return m_enabled;
}

std::filesystem::path DiskTileCache::tilePath(int x, int y, int zoom) const {
    return m_basePath / std::to_string(zoom) / std::to_string(x) / (std::to_string(y) + ".png");
}

std::optional<std::vector<uint8_t>> DiskTileCache::get(int x, int y, int zoom) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto path = tilePath(x, y, zoom);
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return std::nullopt;
    }

    auto size = file.tellg();
    if (size <= 0) {
        return std::nullopt;
    }

    file.seekg(0);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);

    if (!file) {
        return std::nullopt;
    }

    return data;
}

void DiskTileCache::put(int x, int y, int zoom, const void* data, size_t numBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto path = tilePath(x, y, zoom);

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return;
    }

    std::ofstream file(path, std::ios::binary);
    if (file) {
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(numBytes));
    }
}
