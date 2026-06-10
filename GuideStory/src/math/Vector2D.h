#pragma once

// ADR-006: 비즈니스 로직은 SDL 네이티브 타입(SDL_Point 등)에 의존하지 않는다.
// 엔진 자체 2D 벡터 타입을 정의해 그래픽 라이브러리로부터 디커플링한다.
namespace gs::math {

struct Vector2D {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vector2D() = default;
    constexpr Vector2D(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vector2D operator+(const Vector2D& o) const { return {x + o.x, y + o.y}; }
    constexpr Vector2D operator-(const Vector2D& o) const { return {x - o.x, y - o.y}; }
    constexpr Vector2D operator*(float s) const { return {x * s, y * s}; }

    constexpr Vector2D& operator+=(const Vector2D& o) { x += o.x; y += o.y; return *this; }
    constexpr Vector2D& operator-=(const Vector2D& o) { x -= o.x; y -= o.y; return *this; }
};

} // namespace gs::math
