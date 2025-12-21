#pragma once

#include <cstdint>
#include <string>

namespace stream_pipeline {

/*
 * ensure_directory_exists()
 * -------------------------
 * - Stage 0에서는 외부 라이브러리 없이도 output을 생성할 수 있어야 한다.
 * - std::filesystem을 사용한다(ubuntu 22.04 기준 C++17 이상이면 안정적).
 */
bool ensure_directory_exists(const std::string& directory_path);

/*
 * write_ppm_rgb24()
 * -----------------
 * - 외부 이미지 라이브러리 없이 overlay 결과를 파일로 남기기 위한 최소 출력.
 * - PPM(P6) 포맷은 구현이 간단하고 디버깅에 매우 유리하다.
 *
 * 입력:
 * - rgb_data_pointer: width * height * 3 bytes (RGB24)
 */
bool write_ppm_rgb24(
    const std::string& file_path,
    const std::uint8_t* rgb_data_pointer,
    std::int32_t width,
    std::int32_t height);

}  // namespace stream_pipeline
