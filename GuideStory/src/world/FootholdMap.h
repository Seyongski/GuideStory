#pragma once

#include "world/Foothold.h"

#include <vector>

namespace gs::world {

// 맵의 모든 풋홀드(바닥 선분) 집합 + 충돌 질의.
// 1차는 선형 탐색 — 풋홀드 수가 커지면 P-007 QuadTree로 가속(ADR-004).
class FootholdMap {
public:
    void Add(const Foothold& fh) { m_footholds.push_back(fh); }
    void Clear() { m_footholds.clear(); }

    const std::vector<Foothold>& All() const { return m_footholds; }
    std::vector<Foothold>&       All()       { return m_footholds; }

    // 낙하 착지 검출(원웨이): x 수직선에서 발 높이가 [prevFeetY, newFeetY]로
    // 내려오며 표면을 아래로 가로지른 풋홀드 중 가장 위(작은 y)를 반환. 없으면 nullptr.
    const Foothold* FindLanding(float x, float prevFeetY, float newFeetY) const;

    // 보행 중 발밑 풋홀드: x를 범위에 포함하고 표면이 feetY ±tol 이내인 것. 없으면 nullptr.
    const Foothold* GroundAt(float x, float feetY, float tol) const;

    const Foothold* ById(int id) const;

private:
    std::vector<Foothold> m_footholds;
};

} // namespace gs::world
