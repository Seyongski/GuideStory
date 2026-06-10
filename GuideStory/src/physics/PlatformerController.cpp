#include "physics/PlatformerController.h"

namespace gs::physics {

void PlatformerController::Update(const MoveIntent& in, const world::FootholdMap& footholds, float dt) {
    // --- 수평 이동: 가속 + 최고속 클램프, 무입력 시 마찰 ---
    const float accel = m_grounded ? m_cfg.walkAccel : m_cfg.airAccel;
    if (in.moveX != 0.0f) {
        m_vel.x += in.moveX * accel * dt;
        if (m_vel.x >  m_cfg.walkSpeed) m_vel.x =  m_cfg.walkSpeed;
        if (m_vel.x < -m_cfg.walkSpeed) m_vel.x = -m_cfg.walkSpeed;
    } else if (m_grounded) {
        const float fr = m_cfg.friction * dt;
        if (m_vel.x >  fr)      m_vel.x -= fr;
        else if (m_vel.x < -fr) m_vel.x += fr;
        else                    m_vel.x  = 0.0f;
    }

    // --- 드롭다운(↓+점프): 현재 풋홀드를 잠시 통과 ---
    if (in.dropDown && m_grounded) {
        m_grounded = false;
        m_curFoothold = 0;
        m_dropTimer = 0.18f;
        m_pos.y += 2.0f; // 표면 바로 아래로 떨궈 재착지 방지
    }
    if (m_dropTimer > 0.0f) m_dropTimer -= dt;

    // --- 점프: 지상에서만 상승 임펄스 ---
    if (in.jump && m_grounded) {
        m_vel.y = -m_cfg.jumpSpeed;
        m_grounded = false;
        m_curFoothold = 0;
    }

    // --- 중력: 공중일 때만, 종단속도 캡 ---
    if (!m_grounded) {
        m_vel.y += m_cfg.gravity * dt;
        if (m_vel.y > m_cfg.maxFallSpeed) m_vel.y = m_cfg.maxFallSpeed;
    }

    // --- 위치 적분 ---
    const float prevFeetY = m_pos.y;
    m_pos.x += m_vel.x * dt;
    m_pos.y += m_vel.y * dt;

    // --- 착지/보행 판정 ---
    if (m_grounded) {
        // 현재 발밑 풋홀드를 추적(경사 보행). 범위를 벗어나면 낙하 시작.
        const world::Foothold* fh = footholds.GroundAt(m_pos.x, m_pos.y, 8.0f);
        if (fh != nullptr) {
            m_pos.y = fh->SurfaceY(m_pos.x); // 경사면 높이에 스냅
            m_vel.y = 0.0f;
            m_curFoothold = fh->id;
        } else {
            m_grounded = false;
            m_curFoothold = 0;
        }
    } else if (m_vel.y >= 0.0f && m_dropTimer <= 0.0f) {
        // 낙하 중 원웨이 착지: 표면을 위→아래로 가로질렀는가.
        const world::Foothold* land = footholds.FindLanding(m_pos.x, prevFeetY, m_pos.y);
        if (land != nullptr) {
            m_pos.y = land->SurfaceY(m_pos.x);
            m_vel.y = 0.0f;
            m_grounded = true;
            m_curFoothold = land->id;
        }
    }
}

} // namespace gs::physics
