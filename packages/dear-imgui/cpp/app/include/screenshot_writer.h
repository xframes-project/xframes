#pragma once

#include <optional>
#include <string>

std::optional<std::string> WritePngToFile(
    const std::string& path,
    int width,
    int height,
    int channels,
    const unsigned char* data,
    int strideBytes
);
