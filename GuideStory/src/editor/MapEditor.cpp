#include "editor/MapEditor.h"

#include "core/ObjectPalette.h"  // 오브젝트 프리셋(크기·색)
#include "core/WorldRenderer.h"  // PortalBox (히트테스트 박스 공유)
#include "platform/FileDialog.h" // 네이티브 열기/저장 대화상자 + 자산 폴더
#include "world/MapScaffold.h"   // BuildDefaultMap (새 맵)

#include <cmath>
#include <exception>
#include <filesystem>
#include <sstream>

namespace gs::editor {

using platform::Key;
using platform::MouseButton;

namespace {
const char* ModeName(EditMode m) {
    switch (m) {
        case EditMode::Browse:   return "둘러보기";
        case EditMode::Tile:     return "타일";
        case EditMode::Foothold: return "풋홀드";
        case EditMode::Spawn:    return "스폰";
        case EditMode::Portal:   return "포탈";
        case EditMode::Object:   return "오브젝트";
    }
    return "?";
}

// 확장자가 없으면 .gsmap을 붙인다(포탈 대상 맵 이름을 짧게 타이핑하도록).
std::string WithMapExt(std::string name) {
    if (name.find('.') == std::string::npos) name += ".gsmap";
    return name;
}

// 경로에서 파일명만(상태 표시용). UTF-8 바이트 안전(구분자만 탐색).
std::string FileName(const std::string& path) {
    if (path.empty()) return "(저장 안 됨)";
    const auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}
} // namespace

math::Vector2D MapEditor::SnapToGrid(math::Vector2D w) const {
    const float s = static_cast<float>(m_map.Tiles().TileSize());
    return {std::round(w.x / s) * s, std::round(w.y / s) * s};
}

math::Vector2D MapEditor::SnapTopLeft(math::Vector2D w) const {
    const float s = static_cast<float>(m_map.Tiles().TileSize());
    return {std::floor(w.x / s) * s, std::floor(w.y / s) * s};
}

int MapEditor::PortalAt(math::Vector2D world) const {
    const auto& portals = m_map.Portals();
    for (int i = 0; i < static_cast<int>(portals.size()); ++i) {
        if (core::PortalBox(portals[i].pos).Contains(world)) return i;
    }
    return -1;
}

int MapEditor::ObjectAt(math::Vector2D world) const {
    const float ts = static_cast<float>(m_map.Tiles().TileSize());
    const auto& objs = m_map.Objects();
    for (int i = static_cast<int>(objs.size()) - 1; i >= 0; --i) { // 위에 그려진 것 우선
        const core::ObjectPreset& pr = core::ObjectPresetAt(objs[i].preset);
        const math::Rect wr{objs[i].pos.x, objs[i].pos.y, pr.wTiles * ts, pr.hTiles * ts};
        if (wr.Contains(world)) return i;
    }
    return -1;
}

void MapEditor::SetMode(EditMode m) {
    m_mode = m;
    if (m != EditMode::Browse) m_showGrid = true; // 배치 모드는 격자를 켠다
    m_hasPending = false;
    m_panning = false;
    m_status = std::string("모드: ") + ModeName(m_mode);
}

void MapEditor::SetBackground(const std::string& pathOrName) {
    m_map.SetBackground(FileName(pathOrName)); // 파일명만 저장 → assets/backgrounds에서 해석
    m_status = "배경 설정: " + m_map.Background();
}

void MapEditor::SelectObject(int preset) {
    m_currentObject = preset;
    SetMode(EditMode::Object); // 격자 켜짐 + 모드 전환
    m_status = std::string("오브젝트 선택: ") + core::ObjectPresetAt(preset).name;
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

void MapEditor::RecomputeNextPortalId() {
    int maxPt = 0;
    for (const auto& p : m_map.Portals())
        if (p.id > maxPt) maxPt = p.id;
    m_nextPortalId = maxPt + 1; // 포탈이 0개면 1로 리셋 → 다시 #1부터
}

void MapEditor::NewMap() {
    // 먼저 이름(저장 위치)을 정한다. 취소하면 현재 맵을 그대로 둔다.
    const auto path = platform::SaveFileDialog("새 맵 만들기", "GuideStory 맵", "*.gsmap", "gsmap",
                                               platform::AssetsDir("maps"));
    if (!path) { m_status = "새 맵 취소"; return; }
    CreateDefault(*path);
}

void MapEditor::CreateDefault(const std::string& path) {
    world::BuildDefaultMap(m_map);
    RefreshNextIds();
    m_mapPath = path;
    m_mode = EditMode::Browse; // 새 맵은 둘러보기로 시작
    m_panning = false;
    try {
        m_map.Save(m_mapPath); // 결정한 이름으로 즉시 파일 생성
        m_status = "새 맵: " + FileName(m_mapPath);
    } catch (const std::exception& e) {
        m_status = std::string("새 맵 저장 실패: ") + e.what();
    }
    Baseline(); // 갓 만든 기본 맵 = 변경 없음 기준
}

void MapEditor::Save() {
    if (m_mapPath.empty()) { SaveAs(); return; } // 아직 저장한 적 없음 → 위치 지정
    try {
        m_map.Save(m_mapPath);
        m_status = "저장: " + FileName(m_mapPath);
        Baseline(); // 저장 성공 → 변경 없음 기준 갱신
    } catch (const std::exception& e) {
        m_status = std::string("저장 실패: ") + e.what();
    }
}

void MapEditor::SaveAs() {
    const auto path = platform::SaveFileDialog("맵 저장", "GuideStory 맵", "*.gsmap", "gsmap",
                                               platform::AssetsDir("maps"));
    if (!path) { m_status = "저장 취소"; return; }
    try {
        m_map.Save(*path);
        m_mapPath = *path;
        m_status = "저장: " + FileName(m_mapPath);
        Baseline(); // 저장 성공 → 변경 없음 기준 갱신
    } catch (const std::exception& e) {
        m_status = std::string("저장 실패: ") + e.what();
    }
}

void MapEditor::Open() {
    const auto path = platform::OpenFileDialog("맵 열기", "GuideStory 맵", "*.gsmap",
                                               platform::AssetsDir("maps"));
    if (!path) { m_status = "열기 취소"; return; }
    try {
        m_map.Load(*path);
        m_mapPath = *path;
        RefreshNextIds();
        m_mode = EditMode::Browse; // 열면 둘러보기로 시작(클릭이 배치로 새지 않음)
        m_panning = false;
        m_status = "열기: " + FileName(m_mapPath);
        Baseline(); // 막 연 맵 = 변경 없음 기준
    } catch (const std::exception& e) {
        m_status = std::string("열기 실패: ") + e.what();
    }
}

void MapEditor::BeginTextEntry(TextTarget target, std::string initial) {
    m_textActive = true;
    m_textTarget = target;
    m_textBuffer = std::move(initial);
}

void MapEditor::CommitText() {
    std::istringstream ss(m_textBuffer);
    if (m_textTarget == TextTarget::Resize) {
        // 입력은 픽셀. 타일 크기로 나눠 격자 칸 수로 변환한다(배경 PNG에 맞추기 쉽게).
        int wpx = 0, hpx = 0;
        const int ts = m_map.Tiles().TileSize();
        if ((ss >> wpx >> hpx) && wpx > 0 && hpx > 0 && wpx <= 32768 && hpx <= 32768 && ts > 0) {
            const int tw = std::max(1, static_cast<int>(std::lround(static_cast<double>(wpx) / ts)));
            const int th = std::max(1, static_cast<int>(std::lround(static_cast<double>(hpx) / ts)));
            m_map.Tiles().SetSize(tw, th);
            m_status = "맵 크기: " + std::to_string(tw * ts) + " x " + std::to_string(th * ts) + " px";
        } else {
            m_status = "크기 입력 오류 (가로 세로 픽셀)";
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
    }
    m_textActive = false;
    m_textTarget = TextTarget::None;
    m_textBuffer.clear();
}

void MapEditor::Update(const platform::Input& in, core::Camera& cam, float dt, bool allowMouse) {
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

    // --- 격자 토글 / 리사이즈 (모드 전환은 GUI 툴바가 담당) ---
    if (in.WasPressed(Key::G)) m_showGrid = !m_showGrid;
    if (in.WasPressed(Key::R)) {
        const int ts = m_map.Tiles().TileSize();
        BeginTextEntry(TextTarget::Resize,
                       std::to_string(m_map.Tiles().Width() * ts) + " " +
                       std::to_string(m_map.Tiles().Height() * ts));
    }

    // --- 타일 팔레트 선택 (1~5) ---
    if (in.WasPressed(Key::Num1)) m_currentTile = 1;
    if (in.WasPressed(Key::Num2)) m_currentTile = 2;
    if (in.WasPressed(Key::Num3)) m_currentTile = 3;
    if (in.WasPressed(Key::Num4)) m_currentTile = 4;
    if (in.WasPressed(Key::Num5)) m_currentTile = 5;

    const math::Vector2D worldMouse = cam.ScreenToWorld(in.MousePos());
    m_pointer = in.MousePos();          // 오브젝트 고스트 미리보기용
    m_pointerInCanvas = allowMouse;     // GUI 밖일 때만 고스트/배치 허용

    // 포인터가 GUI 툴바 위에 있으면(allowMouse=false) 마우스 편집/드래그를 건너뛴다.
    if (!allowMouse) { m_panning = false; }
    else
    switch (m_mode) {
        case EditMode::Browse: {
            // 좌드래그로 화면 이동(둘러보기). 첫 프레임은 기준점만 잡고 이동하지 않는다.
            if (in.MouseDown(MouseButton::Left)) {
                const math::Vector2D now = in.MousePos();
                // 화면 픽셀 이동량 → 월드 이동량(줌 반영). 잡은 점이 커서에 붙어 따라온다.
                if (m_panning) {
                    const float z = cam.Zoom();
                    cam.Move({(m_panLast.x - now.x) / z, (m_panLast.y - now.y) / z});
                }
                m_panLast = now;
                m_panning = true;
                cam.ClampToBounds(m_map.WorldBounds());
            } else {
                m_panning = false;
            }
            break;
        }
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
            // 좌클릭: 빈 곳=새 포탈, 기존 포탈=선택만. (텍스트 입력에 가두지 않아 마우스가 계속 동작)
            if (in.MousePressed(MouseButton::Left)) {
                const int hit = PortalAt(worldMouse);
                if (hit >= 0) {
                    m_selectedPortal = hit;
                    m_status = "포탈 #" + std::to_string(m_map.Portals()[hit].id) +
                               " 선택 — [Enter] 대상 지정 · 우클릭 삭제";
                } else {
                    world::Portal p;
                    p.id = m_nextPortalId++;
                    p.pos = SnapToGrid(worldMouse);
                    m_map.Portals().push_back(p);
                    m_selectedPortal = static_cast<int>(m_map.Portals().size()) - 1;
                    m_status = "포탈 추가 #" + std::to_string(p.id) + " — [Enter] 대상 지정";
                }
            }
            // 우클릭: 커서 아래 포탈 삭제 + 다음 id 재계산(번호 누적 방지) + 선택 인덱스 보정.
            if (in.MousePressed(MouseButton::Right)) {
                const int hit = PortalAt(worldMouse);
                if (hit >= 0) {
                    m_map.Portals().erase(m_map.Portals().begin() + hit);
                    if (m_selectedPortal == hit)      m_selectedPortal = -1;
                    else if (m_selectedPortal > hit)  --m_selectedPortal; // 뒤 인덱스 당겨짐
                    RecomputeNextPortalId();
                    m_status = "포탈 삭제";
                }
            }
            // Enter: 선택된 포탈의 대상(맵 [포탈id]) 입력 열기.
            if (in.WasPressed(Key::Enter) && m_selectedPortal >= 0 &&
                m_selectedPortal < static_cast<int>(m_map.Portals().size())) {
                const auto& p = m_map.Portals()[m_selectedPortal];
                const std::string init = (p.targetMap.empty() ? std::string("-") : p.targetMap)
                                       + " " + std::to_string(p.targetPortal);
                BeginTextEntry(TextTarget::PortalTarget, init);
            }
            break;
        }
        case EditMode::Object: {
            // 좌클릭: 선택 프리셋을 셀 정렬해 배치. 우클릭: 커서 아래 오브젝트 삭제.
            if (in.MousePressed(MouseButton::Left)) {
                world::MapObject o;
                o.preset = m_currentObject;
                o.pos = SnapTopLeft(worldMouse);
                m_map.Objects().push_back(o);
                m_status = "오브젝트 배치";
            }
            if (in.MousePressed(MouseButton::Right)) {
                const int hit = ObjectAt(worldMouse);
                if (hit >= 0) {
                    m_map.Objects().erase(m_map.Objects().begin() + hit);
                    m_status = "오브젝트 삭제";
                }
            }
            break;
        }
    }

    // --- 저장 / 다른 이름으로 저장 / 열기 / 새 맵 (GUI 툴바와 동일 진입점) ---
    if (in.WasPressed(Key::S)) {
        if (in.IsDown(Key::LShift)) SaveAs(); // Shift+S = 다른 이름으로 저장(대화상자)
        else                        Save();   // S = 현재 파일로 저장
    }
    if (in.WasPressed(Key::L)) Open();        // L = 열기(대화상자)
    if (in.WasPressed(Key::N)) NewMap();      // N = 새 맵(기본 캔버스)
}

void MapEditor::Render(platform::IRenderDevice& r, const core::Camera& cam) const {
    const auto& tm = m_map.Tiles();
    const float s = static_cast<float>(tm.TileSize());

    if (m_showGrid && s > 0.0f) {
        // 격자는 뷰포트(툴바 아래 영역) 안에만 그린다.
        const math::Rect vp = cam.ViewportScreenRect();
        const math::Vector2D topLeft = cam.ScreenToWorld({vp.x, vp.y});
        const platform::Color grid{255, 255, 255, 40};
        const int colsX = static_cast<int>(vp.w / s) + 2;
        const int colsY = static_cast<int>(vp.h / s) + 2;
        const int startX = static_cast<int>(std::floor(topLeft.x / s));
        const int startY = static_cast<int>(std::floor(topLeft.y / s));

        for (int i = 0; i <= colsX; ++i) {
            const float wx = (startX + i) * s;
            const float sx = cam.WorldToScreen({wx, 0.0f}).x;
            r.DrawLine({sx, vp.y}, {sx, vp.Bottom()}, grid);
        }
        for (int j = 0; j <= colsY; ++j) {
            const float wy = (startY + j) * s;
            const float sy = cam.WorldToScreen({0.0f, wy}).y;
            r.DrawLine({vp.x, sy}, {vp.Right(), sy}, grid);
        }
    }

    // 풋홀드 펜딩 첫 점 표시.
    if (m_mode == EditMode::Foothold && m_hasPending) {
        const math::Vector2D p = cam.WorldToScreen(m_pendingPoint);
        r.FillRect({p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f}, {255, 230, 0, 255});
    }

    // 오브젝트 고스트 — 선택 프리셋을 마우스 위치(셀 정렬)에 반투명으로 미리 보여준다.
    if (m_mode == EditMode::Object && m_pointerInCanvas && s > 0.0f) {
        const core::ObjectPreset& pr = core::ObjectPresetAt(m_currentObject);
        const math::Vector2D tl = SnapTopLeft(cam.ScreenToWorld(m_pointer));
        const math::Rect sr = cam.WorldRectToScreen({tl.x, tl.y, pr.wTiles * s, pr.hTiles * s});
        platform::Color ghost = pr.color;
        ghost.a = 120; // 반투명
        r.FillRect(sr, ghost);
        r.DrawRect(sr, {255, 255, 255, 200});
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
    // y는 상단 GUI 툴바(높이 40) 아래에서 시작한다.
    const platform::Color txt{240, 240, 245, 255};
    const char* hint = (m_mode == EditMode::Browse)
        ? "  좌드래그로 화면 이동 · 상단 버튼으로 모드 선택 · [R]맵 크기"
        : "  좌클릭 배치 / 우클릭 취소·삭제 · 상단 버튼으로 모드 선택 · [R]맵 크기";
    r.DrawText(std::string("모드: ") + ModeName(m_mode) + hint, {8.0f, 48.0f}, 20.0f, txt);
    const int ts = tm.TileSize();
    std::string info = "맵: " + FileName(m_mapPath) +
                       "   크기: " + std::to_string(tm.Width() * ts) + " x " +
                       std::to_string(tm.Height() * ts) + " px";
    if (!m_map.Background().empty()) info += "   배경: " + m_map.Background();
    r.DrawText(info, {8.0f, 72.0f}, 20.0f, txt);
    if (!m_status.empty()) r.DrawText(m_status, {8.0f, 96.0f}, 18.0f, {200, 230, 200, 255});

    if (m_textActive) {
        const char* prompt = "";
        switch (m_textTarget) {
            case TextTarget::Resize:       prompt = "맵 크기(가로 세로 픽셀): "; break;
            case TextTarget::PortalTarget: prompt = "대상 맵 [포탈id]: ";   break;
            case TextTarget::None:         break;
        }
        r.DrawText(std::string(prompt) + m_textBuffer + "_",
                   {8.0f, cam.ViewportScreenRect().Bottom() - 40.0f}, 24.0f, {255, 240, 150, 255});
    }
}

} // namespace gs::editor
