#include "editor/MapEditor.h"

#include <cmath>
#include <exception>

namespace gs::editor {

using platform::Key;
using platform::MouseButton;

math::Vector2D MapEditor::SnapToGrid(math::Vector2D w) const {
    const float s = static_cast<float>(m_map.Tiles().TileSize());
    return {std::round(w.x / s) * s, std::round(w.y / s) * s};
}

void MapEditor::Update(const platform::Input& in, core::Camera& cam, float dt) {
    // --- 카메라 패닝(방향키) ---
    const float pan = 700.0f * dt;
    math::Vector2D d{};
    if (in.IsDown(Key::Left))  d.x -= pan;
    if (in.IsDown(Key::Right)) d.x += pan;
    if (in.IsDown(Key::Up))    d.y -= pan;
    if (in.IsDown(Key::Down))  d.y += pan;
    cam.Move(d);

    // --- 토글 ---
    if (in.WasPressed(Key::G)) m_showGrid = !m_showGrid;
    if (in.WasPressed(Key::F)) { m_footholdMode = !m_footholdMode; m_hasPending = false; }

    // --- 타일 팔레트 선택 (1~5) ---
    if (in.WasPressed(Key::Num1)) m_currentTile = 1;
    if (in.WasPressed(Key::Num2)) m_currentTile = 2;
    if (in.WasPressed(Key::Num3)) m_currentTile = 3;
    if (in.WasPressed(Key::Num4)) m_currentTile = 4;
    if (in.WasPressed(Key::Num5)) m_currentTile = 5;

    const math::Vector2D worldMouse = cam.ScreenToWorld(in.MousePos());

    if (m_footholdMode) {
        // 두 점을 찍어 풋홀드(선분) 생성. 그리드에 스냅.
        if (in.MousePressed(MouseButton::Left)) {
            const math::Vector2D p = SnapToGrid(worldMouse);
            if (!m_hasPending) {
                m_pendingPoint = p;
                m_hasPending = true;
            } else {
                world::Foothold fh;
                fh.id = m_nextFootholdId++;
                fh.p1 = m_pendingPoint;
                fh.p2 = p;
                m_map.Footholds().Add(fh);
                m_hasPending = false;
                m_status = "foothold added";
            }
        }
        if (in.MousePressed(MouseButton::Right)) {
            m_hasPending = false; // 첫 점 취소
        }
    } else {
        // 타일 페인트: 좌클릭 칠하기 / 우클릭 지우기 (드래그 지원: IsDown)
        auto& tm = m_map.Tiles();
        const int cx = tm.CellX(worldMouse.x);
        const int cy = tm.CellY(worldMouse.y);
        if (in.MouseDown(MouseButton::Left))  tm.Set(cx, cy, m_currentTile);
        if (in.MouseDown(MouseButton::Right)) tm.Set(cx, cy, world::kEmptyTile);
    }

    // --- 저장 / 로드 (파싱 예외는 상태 문자열로 흡수, 게임은 계속) ---
    if (in.WasPressed(Key::S)) {
        try {
            m_map.Save(m_mapPath);
            m_status = "saved: " + m_mapPath;
        } catch (const std::exception& e) {
            m_status = std::string("save failed: ") + e.what();
        }
    }
    if (in.WasPressed(Key::L)) {
        try {
            m_map.Load(m_mapPath);
            // 다음 풋홀드 id를 기존 최대값+1로 복원.
            int maxId = 0;
            for (const auto& fh : m_map.Footholds().All())
                if (fh.id > maxId) maxId = fh.id;
            m_nextFootholdId = maxId + 1;
            m_hasPending = false;
            m_status = "loaded: " + m_mapPath;
        } catch (const std::exception& e) {
            m_status = std::string("load failed: ") + e.what();
        }
    }
}

void MapEditor::Render(platform::IRenderDevice& r, const core::Camera& cam) const {
    const auto& tm = m_map.Tiles();
    const float s = static_cast<float>(tm.TileSize());

    if (m_showGrid && s > 0.0f) {
        const math::Vector2D topLeft = cam.ScreenToWorld({0.0f, 0.0f});
        const platform::Color grid{255, 255, 255, 40};
        const int colsX = static_cast<int>(cam.ViewW() / s) + 2;
        const int colsY = static_cast<int>(cam.ViewH() / s) + 2;
        const int startX = static_cast<int>(std::floor(topLeft.x / s));
        const int startY = static_cast<int>(std::floor(topLeft.y / s));

        for (int i = 0; i <= colsX; ++i) {
            const float wx = (startX + i) * s;
            const float sx = cam.WorldToScreen({wx, 0.0f}).x;
            r.DrawLine({sx, 0.0f}, {sx, cam.ViewH()}, grid);
        }
        for (int j = 0; j <= colsY; ++j) {
            const float wy = (startY + j) * s;
            const float sy = cam.WorldToScreen({0.0f, wy}).y;
            r.DrawLine({0.0f, sy}, {cam.ViewW(), sy}, grid);
        }
    }

    // 풋홀드 펜딩 첫 점 표시.
    if (m_footholdMode && m_hasPending) {
        const math::Vector2D p = cam.WorldToScreen(m_pendingPoint);
        r.FillRect({p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f}, {255, 230, 0, 255});
    }

    // 현재 모드 표시: 좌상단 작은 배지(풋홀드=노랑, 타일=흰색).
    const platform::Color badge = m_footholdMode
        ? platform::Color{255, 230, 0, 255}
        : platform::Color{255, 255, 255, 255};
    r.FillRect({8.0f, 8.0f, 16.0f, 16.0f}, badge);
}

} // namespace gs::editor
