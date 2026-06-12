#include "editor/MapEditor.h"

#include "core/WorldRenderer.h"  // PortalBox (히트테스트 박스 공유)
#include "world/MapScaffold.h"   // BuildDefaultMap (새 맵)

#include <cmath>
#include <exception>
#include <sstream>

namespace gs::editor {

using platform::Key;
using platform::MouseButton;

namespace {
const char* ModeName(EditMode m) {
    switch (m) {
        case EditMode::Tile:     return "타일";
        case EditMode::Foothold: return "풋홀드";
        case EditMode::Spawn:    return "스폰";
        case EditMode::Portal:   return "포탈";
    }
    return "?";
}

// 확장자가 없으면 .gsmap을 붙인다(맵 이름을 짧게 타이핑하도록).
std::string WithMapExt(std::string name) {
    if (name.find('.') == std::string::npos) name += ".gsmap";
    return name;
}
} // namespace

math::Vector2D MapEditor::SnapToGrid(math::Vector2D w) const {
    const float s = static_cast<float>(m_map.Tiles().TileSize());
    return {std::round(w.x / s) * s, std::round(w.y / s) * s};
}

int MapEditor::PortalAt(math::Vector2D world) const {
    const auto& portals = m_map.Portals();
    for (int i = 0; i < static_cast<int>(portals.size()); ++i) {
        if (core::PortalBox(portals[i].pos).Contains(world)) return i;
    }
    return -1;
}

void MapEditor::CycleMode() {
    switch (m_mode) {
        case EditMode::Tile:     m_mode = EditMode::Foothold; break;
        case EditMode::Foothold: m_mode = EditMode::Spawn;    break;
        case EditMode::Spawn:    m_mode = EditMode::Portal;   break;
        case EditMode::Portal:   m_mode = EditMode::Tile;     break;
    }
    m_hasPending = false;
    m_status = std::string("모드: ") + ModeName(m_mode);
}

void MapEditor::RefreshNextIds() {
    int maxFh = 0;
    for (const auto& fh : m_map.Footholds().All())
        if (fh.id > maxFh) maxFh = fh.id;
    m_nextFootholdId = maxFh + 1;
    int maxPt = 0;
    for (const auto& p : m_map.Portals())
        if (p.id > maxPt) maxPt = p.id;
    m_nextPortalId = maxPt + 1;
    m_hasPending = false;
    m_selectedPortal = -1;
}

void MapEditor::BeginTextEntry(TextTarget target, std::string initial) {
    m_textActive = true;
    m_textTarget = target;
    m_textBuffer = std::move(initial);
}

void MapEditor::CommitText() {
    std::istringstream ss(m_textBuffer);
    if (m_textTarget == TextTarget::Resize) {
        int w = 0, h = 0;
        if ((ss >> w >> h) && w > 0 && h > 0 && w <= 1000 && h <= 1000) {
            m_map.Tiles().SetSize(w, h);
            m_status = "맵 크기 변경: " + std::to_string(w) + "x" + std::to_string(h);
        } else {
            m_status = "크기 입력 오류 (가로 세로)";
        }
    } else if (m_textTarget == TextTarget::PortalTarget) {
        std::string name;
        int tid = 0;
        if (ss >> name) {
            ss >> tid; // 선택 입력(없으면 0 = 대상 스폰)
            if (m_selectedPortal >= 0 && m_selectedPortal < static_cast<int>(m_map.Portals().size())) {
                auto& p = m_map.Portals()[m_selectedPortal];
                p.targetMap = (name == "-") ? std::string() : WithMapExt(name);
                p.targetPortal = tid;
                m_status = "포탈 #" + std::to_string(p.id) + " → " +
                           (p.targetMap.empty() ? "(없음)" : p.targetMap);
            }
        } else {
            m_status = "대상 입력 오류 (파일명 [포탈id])";
        }
    } else if (m_textTarget == TextTarget::SaveAs) {
        std::string name;
        if (ss >> name) {
            m_mapPath = WithMapExt(name);     // 이름 지정/변경 → 현재 맵 이름이 됨
            try {
                m_map.Save(m_mapPath);
                m_status = "저장: " + m_mapPath;
            } catch (const std::exception& e) {
                m_status = std::string("저장 실패: ") + e.what();
            }
        } else {
            m_status = "이름 입력 오류";
        }
    } else if (m_textTarget == TextTarget::Open) {
        std::string name;
        if (ss >> name) {
            const std::string path = WithMapExt(name);
            try {
                m_map.Load(path);
                m_mapPath = path;             // 현재 편집 대상이 됨
                RefreshNextIds();
                m_status = "열기: " + m_mapPath;
            } catch (const std::exception& e) {
                m_status = std::string("열기 실패: ") + e.what();
            }
        } else {
            m_status = "이름 입력 오류";
        }
    }
    m_textActive = false;
    m_textTarget = TextTarget::None;
    m_textBuffer.clear();
}

void MapEditor::Update(const platform::Input& in, core::Camera& cam, float dt) {
    // --- 텍스트 입력 중에는 버퍼만 편집하고 다른 입력은 무시한다 ---
    if (m_textActive) {
        m_textBuffer += in.TextInput();
        if (in.WasPressed(Key::Backspace) && !m_textBuffer.empty()) m_textBuffer.pop_back();
        if (in.WasPressed(Key::Enter))  CommitText();
        if (in.WasPressed(Key::Escape)) {           // 취소
            m_textActive = false; m_textTarget = TextTarget::None;
            m_textBuffer.clear(); m_status = "입력 취소";
        }
        return;
    }

    // --- 카메라 패닝(방향키) + 경계 클램프 ---
    const float pan = 700.0f * dt;
    math::Vector2D d{};
    if (in.IsDown(Key::Left))  d.x -= pan;
    if (in.IsDown(Key::Right)) d.x += pan;
    if (in.IsDown(Key::Up))    d.y -= pan;
    if (in.IsDown(Key::Down))  d.y += pan;
    cam.Move(d);
    cam.ClampToBounds(m_map.WorldBounds());

    // --- 토글 / 모드 / 리사이즈 ---
    if (in.WasPressed(Key::G))   m_showGrid = !m_showGrid;
    if (in.WasPressed(Key::Tab)) CycleMode();
    if (in.WasPressed(Key::R)) {
        BeginTextEntry(TextTarget::Resize,
                       std::to_string(m_map.Tiles().Width()) + " " +
                       std::to_string(m_map.Tiles().Height()));
    }

    // --- 타일 팔레트 선택 (1~5) ---
    if (in.WasPressed(Key::Num1)) m_currentTile = 1;
    if (in.WasPressed(Key::Num2)) m_currentTile = 2;
    if (in.WasPressed(Key::Num3)) m_currentTile = 3;
    if (in.WasPressed(Key::Num4)) m_currentTile = 4;
    if (in.WasPressed(Key::Num5)) m_currentTile = 5;

    const math::Vector2D worldMouse = cam.ScreenToWorld(in.MousePos());

    switch (m_mode) {
        case EditMode::Tile: {
            // 타일 페인트: 좌클릭 칠하기 / 우클릭 지우기 (드래그 지원: IsDown)
            auto& tm = m_map.Tiles();
            const int cx = tm.CellX(worldMouse.x);
            const int cy = tm.CellY(worldMouse.y);
            if (in.MouseDown(MouseButton::Left))  tm.Set(cx, cy, m_currentTile);
            if (in.MouseDown(MouseButton::Right)) tm.Set(cx, cy, world::kEmptyTile);
            break;
        }
        case EditMode::Foothold: {
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
                    m_status = "풋홀드 추가";
                }
            }
            if (in.MousePressed(MouseButton::Right)) m_hasPending = false; // 첫 점 취소
            break;
        }
        case EditMode::Spawn: {
            if (in.MousePressed(MouseButton::Left)) {
                m_map.SetSpawn(worldMouse);
                m_status = "스폰 위치 설정";
            }
            break;
        }
        case EditMode::Portal: {
            if (in.MousePressed(MouseButton::Left)) {
                const int hit = PortalAt(worldMouse);
                if (hit >= 0) {
                    // 기존 포탈 선택 → 대상 입력 열기(현재 값 미리 채움).
                    m_selectedPortal = hit;
                    const auto& p = m_map.Portals()[hit];
                    const std::string init = (p.targetMap.empty() ? std::string("-") : p.targetMap)
                                           + " " + std::to_string(p.targetPortal);
                    BeginTextEntry(TextTarget::PortalTarget, init);
                } else {
                    // 빈 곳 → 새 포탈(대상 비움).
                    world::Portal p;
                    p.id = m_nextPortalId++;
                    p.pos = SnapToGrid(worldMouse);
                    m_map.Portals().push_back(p);
                    m_selectedPortal = static_cast<int>(m_map.Portals().size()) - 1;
                    m_status = "포탈 추가 #" + std::to_string(p.id) + " (클릭해 대상 지정)";
                }
            }
            if (in.MousePressed(MouseButton::Right)) {
                const int hit = PortalAt(worldMouse);
                if (hit >= 0) {
                    m_map.Portals().erase(m_map.Portals().begin() + hit);
                    if (m_selectedPortal == hit) m_selectedPortal = -1;
                    m_status = "포탈 삭제";
                }
            }
            break;
        }
    }

    // --- 저장 / 다른 이름으로 저장 / 열기 / 새 맵 ---
    if (in.WasPressed(Key::S)) {
        if (in.IsDown(Key::LShift)) {
            BeginTextEntry(TextTarget::SaveAs, m_mapPath); // Shift+S = 다른 이름으로 저장(이름 변경)
        } else {
            try {                                          // S = 현재 이름으로 저장
                m_map.Save(m_mapPath);
                m_status = "저장: " + m_mapPath;
            } catch (const std::exception& e) {
                m_status = std::string("저장 실패: ") + e.what();
            }
        }
    }
    if (in.WasPressed(Key::L)) {
        BeginTextEntry(TextTarget::Open, m_mapPath);       // L = 이름으로 열기(현재 이름 미리 채움)
    }
    if (in.WasPressed(Key::N)) {                            // N = 새 맵(기본 캔버스)
        world::BuildDefaultMap(m_map);
        RefreshNextIds();
        m_mapPath = "untitled.gsmap";
        m_status = "새 맵 — Shift+S로 이름을 지정하세요";
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
    if (m_mode == EditMode::Foothold && m_hasPending) {
        const math::Vector2D p = cam.WorldToScreen(m_pendingPoint);
        r.FillRect({p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f}, {255, 230, 0, 255});
    }

    // 스폰 마커 — 청록 깃발(세로선 + 머리).
    {
        const math::Vector2D sp = cam.WorldToScreen(m_map.Spawn());
        const platform::Color cyan{60, 220, 220, 255};
        r.DrawLine({sp.x, sp.y}, {sp.x, sp.y - 40.0f}, cyan);
        r.FillRect({sp.x, sp.y - 40.0f, 22.0f, 14.0f}, cyan);
        r.FillRect({sp.x - 3.0f, sp.y - 3.0f, 6.0f, 6.0f}, cyan);
    }

    // 포탈 라벨(번호 → 대상)과 선택 강조.
    const auto& portals = m_map.Portals();
    for (int i = 0; i < static_cast<int>(portals.size()); ++i) {
        const auto& p = portals[i];
        const math::Rect sr = cam.WorldRectToScreen(core::PortalBox(p.pos));
        if (i == m_selectedPortal) r.DrawRect({sr.x - 2, sr.y - 2, sr.w + 4, sr.h + 4},
                                              {255, 230, 0, 255});
        const std::string label = "#" + std::to_string(p.id) + (p.targetMap.empty() ? "" : "→" + p.targetMap);
        r.DrawText(label, {sr.x, sr.y - 20.0f}, 16.0f, {235, 235, 255, 255});
    }

    // 좌상단 상태 텍스트(모드/맵이름·크기/상태/입력 프롬프트).
    const platform::Color txt{240, 240, 245, 255};
    r.DrawText(std::string("모드: ") + ModeName(m_mode) +
                   "  [Tab]전환 [R]크기 [G]격자 [S]저장 [Shift+S]다른이름 [L]열기 [N]새맵",
               {8.0f, 8.0f}, 20.0f, txt);
    r.DrawText("맵: " + m_mapPath +
                   "   크기: " + std::to_string(tm.Width()) + " x " + std::to_string(tm.Height()),
               {8.0f, 32.0f}, 20.0f, txt);
    if (!m_status.empty()) r.DrawText(m_status, {8.0f, 56.0f}, 18.0f, {200, 230, 200, 255});

    if (m_textActive) {
        const char* prompt = "";
        switch (m_textTarget) {
            case TextTarget::Resize:       prompt = "맵 크기(가로 세로): "; break;
            case TextTarget::PortalTarget: prompt = "대상 맵 [포탈id]: ";   break;
            case TextTarget::SaveAs:       prompt = "다른 이름으로 저장: "; break;
            case TextTarget::Open:         prompt = "맵 열기(이름): ";       break;
            case TextTarget::None:         break;
        }
        r.DrawText(std::string(prompt) + m_textBuffer + "_",
                   {8.0f, cam.ViewH() - 40.0f}, 24.0f, {255, 240, 150, 255});
    }
}

} // namespace gs::editor
