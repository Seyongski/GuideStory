#pragma once

#include "math/Rect.h"
#include "math/Vector2D.h"

#include <algorithm>
#include <cmath>

namespace gs::core {

// 월드 좌표 ↔ 화면(픽셀) 좌표 변환. 렌더 디바이스는 화면 좌표만 받으므로
// 스크롤/추적은 여기서 처리한다(ADR-006: 렌더 디바이스는 변환을 모른다).
class Camera {
public:
    // viewW/viewH: 뷰포트(월드를 그리는 화면 영역)의 크기.
    // offsetX/offsetY: 그 뷰포트의 화면 좌상단 위치(기본 0,0). 에디터는 상단 툴바 높이만큼
    //   아래로 밀어, 월드가 툴바 아래부터 그려지고 picking도 그에 맞게 변환되도록 한다.
    Camera(float viewW, float viewH, float offsetX = 0.0f, float offsetY = 0.0f)
        : m_viewW(viewW), m_viewH(viewH), m_offX(offsetX), m_offY(offsetY) {}

    // 카메라를 target에 즉시 맞춘다(데드존 무시). 스폰·포탈·부활 등 순간이동에 사용.
    void SnapTo(math::Vector2D target) { m_center = target; }

    // 데드존 추적: target이 중심 기준 데드존 사각형 '밖으로' 나간 만큼만 카메라가 따라간다.
    // 안에 있으면 카메라 정지 — 플레이어가 화면 중앙 영역에서 움직여도 화면이 흔들리지 않는다.
    void Follow(math::Vector2D target) {
        // 데드존은 화면 픽셀 → 월드 단위로 환산(zoom 반영). zoom=1이면 기존과 동일.
        const float deadW = m_deadHalfW / m_zoom;
        const float deadH = m_deadHalfH / m_zoom;
        const float dx = target.x - m_center.x;
        if (dx >  deadW)      m_center.x += dx - deadW;
        else if (dx < -deadW) m_center.x += dx + deadW;
        const float dy = target.y - m_center.y;
        if (dy >  deadH)      m_center.y += dy - deadH;
        else if (dy < -deadH) m_center.y += dy + deadH;
    }

    // 데드존 + 카메라 렉(지수 스무딩). 데드존 '목표 중심' G로 즉시 가지 않고 매 프레임 일부만 다가간다.
    //  - k(1/초): 응답성. 클수록 렉↓(빨리 따라잡음). 게임층이 이동 속도로 계산해 넘긴다.
    //  - a = 1 - exp(-k·dt): 프레임레이트 독립 보간 계수(Exponential Out 곡선). 상수 lerp는 fps에 따라
    //    렉이 달라지므로 쓰지 않는다. 별도 속도 상태가 없어 SnapTo(순간이동)와도 충돌 없음.
    void FollowLagged(math::Vector2D target, float dt, float k) {
        const float deadW = m_deadHalfW / m_zoom;
        const float deadH = m_deadHalfH / m_zoom;
        const float gx = GoalCenter1D(m_center.x, target.x, deadW);
        const float gy = GoalCenter1D(m_center.y, target.y, deadH);
        const float a = 1.0f - std::exp(-k * dt);
        m_center.x += (gx - m_center.x) * a;
        m_center.y += (gy - m_center.y) * a;
    }

    void Move(math::Vector2D delta) { m_center += delta; } // 에디터 패닝

    // 줌 배율(화면 = 월드 * zoom). 1=원본, >1 확대(좁은 영역이 화면을 채움), <1 축소.
    // 게임: 파란 사각형(화면 영역)을 창 크기로 채우는 배율 = kViewW/파랑폭. 에디터: 둘러보기 확인용 배율.
    // 월드 데이터·물리는 zoom과 무관 — 순수 화면 변환이라 안전하다.
    void  SetZoom(float z) { m_zoom = (z > 0.01f) ? z : 0.01f; }
    float Zoom() const     { return m_zoom; }

    // 데드존 반-크기(화면 픽셀) 주입. 기본은 전역 손맛값. 맵별 카메라가 필요해지면 여기로.
    void SetDeadzone(float halfW, float halfH) { m_deadHalfW = halfW; m_deadHalfH = halfH; }

    // 뷰가 월드 경계 밖으로 나가지 않게 center를 보정한다(무한맵 느낌 해소).
    // 월드가 뷰보다 작은 축은 월드 중앙에 고정.
    void ClampToBounds(const math::Rect& world) {
        // 화면에 들어오는 월드 범위 = viewW/zoom × viewH/zoom. 그 사각형이 world 안에 머물도록 center 보정.
        const float viewWWorld = m_viewW / m_zoom;
        const float viewHWorld = m_viewH / m_zoom;
        const float halfW = viewWWorld * 0.5f;
        const float halfH = viewHWorld * 0.5f;
        if (world.w <= viewWWorld) m_center.x = world.x + world.w * 0.5f;
        else m_center.x = std::clamp(m_center.x, world.Left() + halfW, world.Right() - halfW);
        if (world.h <= viewHWorld) m_center.y = world.y + world.h * 0.5f;
        else m_center.y = std::clamp(m_center.y, world.Top() + halfH, world.Bottom() - halfH);
    }

    math::Vector2D Center() const { return m_center; }
    float ViewW() const { return m_viewW; }
    float ViewH() const { return m_viewH; }

    // 월드를 그리는 화면 영역(컬링·클립 계산용). 오프셋이 0이면 {0,0,viewW,viewH}.
    math::Rect ViewportScreenRect() const { return {m_offX, m_offY, m_viewW, m_viewH}; }

    math::Vector2D WorldToScreen(math::Vector2D w) const {
        return {(w.x - m_center.x) * m_zoom + m_offX + m_viewW * 0.5f,
                (w.y - m_center.y) * m_zoom + m_offY + m_viewH * 0.5f};
    }
    math::Vector2D ScreenToWorld(math::Vector2D s) const {
        return {(s.x - m_offX - m_viewW * 0.5f) / m_zoom + m_center.x,
                (s.y - m_offY - m_viewH * 0.5f) / m_zoom + m_center.y};
    }
    math::Rect WorldRectToScreen(const math::Rect& r) const {
        const math::Vector2D p = WorldToScreen({r.x, r.y});
        return {p.x, p.y, r.w * m_zoom, r.h * m_zoom};
    }

    // 데드존 목표 중심(1축): 목표가 데드존 밖이면 가장자리에 맞춘 중심, 안이면 현재 중심(정지).
    static float GoalCenter1D(float center, float target, float deadHalf) {
        const float d = target - center;
        if (d >  deadHalf) return target - deadHalf;
        if (d < -deadHalf) return target + deadHalf;
        return center;
    }

private:
    // 데드존 기본값(화면 픽셀, 중심 기준 반-크기). 게임 손맛이라 전역 동일 — 맵 데이터가 아니다.
    // 맵별 override가 필요한 통증(보스방 카메라 고정 등)이 생기면 그때 .gsmap 직렬화로 승격.
    static constexpr float kDeadzoneHalfW = 110.0f;
    static constexpr float kDeadzoneHalfH = 90.0f;

    float m_viewW;
    float m_viewH;
    float m_offX = 0.0f;
    float m_offY = 0.0f;
    math::Vector2D m_center{};
    float m_zoom = 1.0f; // 화면=월드*zoom. 1=원본.
    float m_deadHalfW = kDeadzoneHalfW;
    float m_deadHalfH = kDeadzoneHalfH;
};

} // namespace gs::core
