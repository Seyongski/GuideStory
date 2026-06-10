#pragma once

#include <cstdint>

namespace gs::platform {

// 그래픽 라이브러리 비의존 색상 표현 (ADR-006).
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

} // namespace gs::platform
