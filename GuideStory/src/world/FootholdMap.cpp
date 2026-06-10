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

const Foothold* FootholdMap::ById(int id) const {
    for (const auto& fh : m_footholds) {
        if (fh.id == id) return &fh;
    }
    return nullptr;
}

} // namespace gs::world
