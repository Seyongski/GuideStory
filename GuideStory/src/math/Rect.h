#pragma once

#include "math/Vector2D.h"

// ADR-006: SDL_Rect에 의존하지 않는 자체 사각형 타입.
// ADR-004: AABB 충돌 판정의 기본 단위.
namespace gs::math {

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    constexpr Rect() = default;
    constexpr Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}

    constexpr float Left()   const { return x; }
    constexpr float Right()  const { return x + w; }
    constexpr float Top()    const { return y; }
    constexpr float Bottom() const { return y + h; }

    constexpr Vector2D Position() const { return {x, y}; }
    constexpr Vector2D Size()     const { return {w, h}; }

    // AABB 교차 판정 (ADR-004 충돌 처리의 기초).
    constexpr bool Intersects(const Rect& o) const {
        return Left() < o.Right() && Right() > o.Left() &&
               Top() < o.Bottom() && Bottom() > o.Top();
    }

    constexpr bool Contains(const Vector2D& p) const {
        return p.x >= Left() && p.x < Right() && p.y >= Top() && p.y < Bottom();
    }
};

} // namespace gs::math
