#pragma once

#include "math/Vector2D.h"
#include "world/FootholdMap.h"

// 메이플식 플랫포머 물리. SDL 무관(ADR-006).
// 입력은 platform::Input이 아니라 추상 MoveIntent로 받아 platform과도 디커플링한다.
namespace gs::physics {

// 손맛 튜닝값 (픽셀/초 단위). 추후 데이터 주도(JSON)로 분리 가능(ADR-005).
struct PlatformerConfig {
    float gravity      = 2000.0f; // 중력 가속
    float maxFallSpeed = 900.0f;  // 낙하 종단속도
    float walkSpeed    = 260.0f;  // 지상 최고 수평속도
    float walkAccel    = 3200.0f; // 지상 가속
    float airAccel     = 1800.0f; // 공중 제어(약하게)
    float friction     = 3200.0f; // 지상 마찰 감속
    float jumpSpeed    = 730.0f;  // 점프 초기 상승속도(임펄스)
};

// 이번 프레임의 이동 의도. 게임 층이 platform::Input을 번역해 채운다.
struct MoveIntent {
    float moveX    = 0.0f; // -1(좌) .. +1(우)
    bool  jump     = false; // 점프 눌림(엣지)
    bool  dropDown = false; // ↓+점프 드롭다운(엣지)
};

// 발 기준점(x=중심, y=바닥)을 가진 운동학적 바디.
class PlatformerController {
public:
    explicit PlatformerController(PlatformerConfig cfg = {}) : m_cfg(cfg) {}

    void SetPosition(math::Vector2D feet) {
        m_pos = feet; m_vel = {}; m_grounded = false; m_curFoothold = 0; m_dropTimer = 0.0f;
    }

    // 발 x를 [minX, maxX]로 가둔다(맵 좌우 벽). 클램프되면 수평 속도만 제거해
    // 벽에 붙어 멈추되, 착지/수직 상태(grounded, foothold, vy)는 보존한다.
    void ClampX(float minX, float maxX) {
        if (m_pos.x < minX) { m_pos.x = minX; if (m_vel.x < 0.0f) m_vel.x = 0.0f; }
        else if (m_pos.x > maxX) { m_pos.x = maxX; if (m_vel.x > 0.0f) m_vel.x = 0.0f; }
    }

    math::Vector2D Position() const { return m_pos; } // 발 기준점
    math::Vector2D Velocity() const { return m_vel; }
    bool           Grounded() const { return m_grounded; }
    int            FootholdId() const { return m_curFoothold; }

    // 입력·풋홀드·dt로 한 스텝 적분 + 착지/보행 판정.
    void Update(const MoveIntent& in, const world::FootholdMap& footholds, float dt);

private:
    PlatformerConfig m_cfg;
    math::Vector2D   m_pos{};
    math::Vector2D   m_vel{};
    bool             m_grounded = false;
    int              m_curFoothold = 0;
    float            m_dropTimer = 0.0f; // >0이면 풋홀드 무시(드롭다운 직후)
};

} // namespace gs::physics
