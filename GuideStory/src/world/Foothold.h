#pragma once

#include "math/Vector2D.h"

#include <algorithm>

// 메이플식 충돌의 기본 단위: 바닥 "선분"(원웨이 플랫폼).
// 칸 단위 솔리드 블록이 아니라 선분이므로 경사·원웨이·드롭다운이 자연스럽다.
namespace gs::world {

struct Foothold {
    int id = 0;
    math::Vector2D p1;   // 한쪽 끝
    math::Vector2D p2;   // 반대쪽 끝
    int prev = 0;        // 연결된 풋홀드 id (0 = 없음) — 보행 체인용(후속 확장)
    int next = 0;

    float MinX() const { return std::min(p1.x, p2.x); }
    float MaxX() const { return std::max(p1.x, p2.x); }

    // x가 이 풋홀드의 수평 범위 안에 있는가.
    bool ContainsX(float x) const { return x >= MinX() && x <= MaxX(); }

    // 수직 선분(벽). 바닥 표면 질의에서 제외한다(후속: 벽 막힘 처리).
    bool IsWall() const { return p1.x == p2.x; }

    // x에서의 표면 높이(y). 선형 보간 → 경사 보행을 지원한다.
    float SurfaceY(float x) const {
        const float dx = p2.x - p1.x;
        if (dx == 0.0f) return p1.y; // 안전장치 (IsWall이면 호출 전 걸러짐)
        const float t = (x - p1.x) / dx;
        return p1.y + (p2.y - p1.y) * t;
    }
};

} // namespace gs::world
