#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "screenshot_writer.h"

#include <fmt/core.h>

#include <limits>

std::optional<std::string> WritePngToFile(
    const std::string& path,
    const int width,
    const int height,
    const int channels,
    const unsigned char* data,
    const int strideBytes
) {
    if (path.empty()) {
        return "Screenshot path is empty";
    }

    if (width <= 0 || height <= 0) {
        return fmt::format("Invalid screenshot dimensions: {}x{}", width, height);
    }

    if (channels <= 0 || channels > 4) {
        return fmt::format("Invalid screenshot channel count: {}", channels);
    }

    if (data == nullptr) {
        return "Screenshot pixel buffer is null";
    }

    if (width > std::numeric_limits<int>::max() / channels) {
        return "Screenshot row size exceeds the supported range";
    }

    const int minimumStrideBytes = width * channels;
    if (strideBytes < minimumStrideBytes) {
        return fmt::format(
            "Invalid screenshot stride: {} bytes; expected at least {}",
            strideBytes,
            minimumStrideBytes
        );
    }

    if (stbi_write_png(path.c_str(), width, height, channels, data, strideBytes) == 0) {
        return fmt::format("Failed to write PNG screenshot to '{}'", path);
    }

    return std::nullopt;
}
