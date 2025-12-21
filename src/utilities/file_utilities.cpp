#include "stream_pipeline/utilities/file_utilities.hpp"

#include <filesystem>
#include <fstream>

namespace stream_pipeline {

bool ensure_directory_exists(const std::string& directory_path) {
    try {
        if (directory_path.empty()) {
            return false;
        }
        std::filesystem::path path(directory_path);
        if (std::filesystem::exists(path)) {
            return std::filesystem::is_directory(path);
        }
        return std::filesystem::create_directories(path);
    } catch (...) {
        return false;
    }
}

bool write_ppm_rgb24(
    const std::string& file_path,
    const std::uint8_t* rgb_data_pointer,
    std::int32_t width,
    std::int32_t height) {

    if (rgb_data_pointer == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    std::ofstream output_stream(file_path, std::ios::binary);
    if (!output_stream.is_open()) {
        return false;
    }

    // PPM P6 header
    // - "P6"
    // - "width height"
    // - "255"
    output_stream << "P6\n" << width << " " << height << "\n255\n";

    const std::size_t byte_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
    output_stream.write(reinterpret_cast<const char*>(rgb_data_pointer), static_cast<std::streamsize>(byte_count));

    return output_stream.good();
}

}  // namespace stream_pipeline
