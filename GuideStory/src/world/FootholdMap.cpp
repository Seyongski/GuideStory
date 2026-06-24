#include "world/FootholdMap.h"

#include <cmath>

namespace gs::world {

const Foothold* FootholdMap::FindLanding(float x, float prevFeetY, float newFeetY) const {
    const Foothold* best = nullptr;
    float bestY = 0.0f;
    constexpr float eps = 1.0f; // 직전 프레임에 표면 살짝 아래여도 착지로 인정

    for (const auto& fh : m_footholds) {
        if (fh.IsWall() || !fh.ContainsX(x)) continue;
        const float surfY = fh.SurfaceY(x);
        // 위→아래로 표면을 가로질렀는가: 직전 발높이가 표면 위(<=)였고, 새 발높이가 표면 도달/통과.
        if (prevFeetY <= surfY + eps && newFeetY >= surfY) {
            if (best == nullptr || surfY < bestY) {
                best = &fh;
                bestY = surfY;
            }
        }
    }
    return best;
}

const Foothold* FootholdMap::GroundAt(float x, float feetY, float tol) const {
    const Foothold* best = nullptr;
    float bestDy = tol;

    for (const auto& fh : m_footholds) {
        if (fh.IsWall() || !fh.ContainsX(x)) continue;
        const float dy = std::fabs(fh.SurfaceY(x) - feetY);
        if (dy <= bestDy) {
            best = &fh;
            bestDy = dy;
        }
    }
    return best;
}

float FootholdMap::BlockHorizontal(float prevX, float newX, float halfW,
                                   float topY, float bottomY) const {
    float result = newX;
    for (const auto& fh : m_footholds) {
        if (!fh.IsWall()) continue;
        const float xw = fh.p1.x;
        const float wMinY = std::min(fh.p1.y, fh.p2.y);
        const float wMaxY = std::max(fh.p1.y, fh.p2.y);
        // 수직 겹침: 몸통 [topY,bottomY]가 벽 세로범위와 겹쳐야 막는다(아래/위로 빗나가면 통과).
        if (bottomY <= wMinY || topY >= wMaxY) continue;
        if (newX > prevX) {                       // 오른쪽으로 이동: 오른쪽 모서리로 충돌
            if (prevX + halfW <= xw && newX + halfW > xw)
                result = std::min(result, xw - halfW);
        } else if (newX < prevX) {                // 왼쪽으로 이동: 왼쪽 모서리로 충돌
            if (prevX - halfW >= xw && newX - halfW < xw)
                result = std::max(result, xw + halfW);
        }
    }
    return result;
}

const Foothold* FootholdMap::ById(int id) const {
    for (const auto& fh : m_footholds) {
        if (fh.id == id) return &fh;
    }
    return nullptr;
}

} // namespace gs::world
