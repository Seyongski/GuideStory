#pragma once

#include "math/Rect.h"
#include "math/Vector2D.h"

#include <algorithm>

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
        const float dx = target.x - m_center.x;
        if (dx >  m_deadHalfW)      m_center.x += dx - m_deadHalfW;
        else if (dx < -m_deadHalfW) m_center.x += dx + m_deadHalfW;
        const float dy = target.y - m_center.y;
        if (dy >  m_deadHalfH)      m_center.y += dy - m_deadHalfH;
        else if (dy < -m_deadHalfH) m_center.y += dy + m_deadHalfH;
    }

    void Move(math::Vector2D delta) { m_center += delta; } // 에디터 패닝

    // 데드존 반-크기(화면 픽셀) 주입. 기본은 전역 손맛값. 맵별 카메라가 필요해지면 여기로.
    void SetDeadzone(float halfW, float halfH) { m_deadHalfW = halfW; m_deadHalfH = halfH; }

    // 뷰가 월드 경계 밖으로 나가지 않게 center를 보정한다(무한맵 느낌 해소).
    // 월드가 뷰보다 작은 축은 월드 중앙에 고정.
    void ClampToBounds(const math::Rect& world) {
        const float halfW = m_viewW * 0.5f;
        const float halfH = m_viewH * 0.5f;
        if (world.w <= m_viewW) m_center.x = world.x + world.w * 0.5f;
        else m_center.x = std::clamp(m_center.x, world.Left() + halfW, world.Right() - halfW);
        if (world.h <= m_viewH) m_center.y = world.y + world.h * 0.5f;
        else m_center.y = std::clamp(m_center.y, world.Top() + halfH, world.Bottom() - halfH);
    }

    math::Vector2D Center() const { return m_center; }
    float ViewW() const { return m_viewW; }
    float ViewH() const { return m_viewH; }

    // 월드를 그리는 화면 영역(컬링·클립 계산용). 오프셋이 0이면 {0,0,viewW,viewH}.
    math::Rect ViewportScreenRect() const { return {m_offX, m_offY, m_viewW, m_viewH}; }

    math::Vector2D WorldToScreen(math::Vector2D w) const {
        return {w.x - (m_center.x - m_viewW * 0.5f) + m_offX,
                w.y - (m_center.y - m_viewH * 0.5f) + m_offY};
    }
    math::Vector2D ScreenToWorld(math::Vector2D s) const {
        return {s.x - m_offX + (m_center.x - m_viewW * 0.5f),
                s.y - m_offY + (m_center.y - m_viewH * 0.5f)};
    }
    math::Rect WorldRectToScreen(const math::Rect& r) const {
        const math::Vector2D p = WorldToScreen({r.x, r.y});
        return {p.x, p.y, r.w, r.h};
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
    float m_deadHalfW = kDeadzoneHalfW;
    float m_deadHalfH = kDeadzoneHalfH;
};

} // namespace gs::core
