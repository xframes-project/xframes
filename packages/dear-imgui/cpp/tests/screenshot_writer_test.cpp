#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "screenshot_writer.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using ::testing::HasSubstr;

namespace {
    std::filesystem::path MakeTemporaryPngPath() {
        const auto uniqueValue = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path()
            / ("xframes-screenshot-writer-" + std::to_string(uniqueValue) + ".png");
    }
}

TEST(ScreenshotWriter, RejectsInvalidArguments) {
    const std::array<unsigned char, 4> pixel{255, 0, 0, 255};

    EXPECT_THAT(WritePngToFile("", 1, 1, 4, pixel.data(), 4).value(), HasSubstr("path is empty"));
    EXPECT_THAT(WritePngToFile("unused.png", 0, 1, 4, pixel.data(), 4).value(), HasSubstr("dimensions"));
    EXPECT_THAT(WritePngToFile("unused.png", 1, 1, 5, pixel.data(), 5).value(), HasSubstr("channel count"));
    EXPECT_THAT(WritePngToFile("unused.png", 1, 1, 4, nullptr, 4).value(), HasSubstr("buffer is null"));
    EXPECT_THAT(WritePngToFile("unused.png", 1, 1, 4, pixel.data(), 3).value(), HasSubstr("stride"));
}

TEST(ScreenshotWriter, WritesPngWithExpectedSignature) {
    const auto path = MakeTemporaryPngPath();
    const std::array<unsigned char, 16> pixels{
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255,
    };

    const auto maybeError = WritePngToFile(path.string(), 2, 2, 4, pixels.data(), 8);
    ASSERT_FALSE(maybeError.has_value()) << maybeError.value_or("");
    ASSERT_TRUE(std::filesystem::exists(path));
    ASSERT_GT(std::filesystem::file_size(path), 8U);

    std::ifstream input(path, std::ios::binary);
    ASSERT_TRUE(input.is_open());

    std::array<unsigned char, 8> signature{};
    input.read(reinterpret_cast<char*>(signature.data()), static_cast<std::streamsize>(signature.size()));

    constexpr std::array<unsigned char, 8> expectedSignature{
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
    };
    EXPECT_EQ(signature, expectedSignature);

    input.close();
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    EXPECT_FALSE(removeError) << removeError.message();
}
