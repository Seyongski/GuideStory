#include "MapEditorScreen.h"

#include "core/ObjectPalette.h"
#include "core/WorldRenderer.h"
#include "platform/FileDialog.h"
#include "world/MapScaffold.h"

#include <algorithm>
#include <cmath>

namespace gs::app {

namespace {
constexpr platform::Color kEmptyBg {30, 34, 52, 255};
constexpr platform::Color kSky     {100, 149, 237, 255};
constexpr platform::Color kTitle   {236, 224, 150, 255};
constexpr platform::Color kHint    {150, 160, 180, 255};
constexpr platform::Color kBar     {20, 24, 38, 220}; // 상단 툴바 배경
constexpr platform::Color kPanel   {26, 30, 46, 240}; // 우측 팔레트 패널 배경
constexpr float kPanelW = 210.0f;
constexpr float kPanelX = kViewW - kPanelW; // 우측 패널 좌측 경계

enum EmptyItem { kNew0 = 0, kOpen0, kBack0 };
enum ToolItem  { kBrowse = 0, kGrid, kTile, kFoothold, kSpawn, kPortal, kBackground, kObject };
enum FileItem  { kNew = 0, kOpen, kSave, kSaveAs, kBack };

// 현재 편집 모드에 대응하는 모드 툴바 버튼 인덱스(활성 강조용).
int ModeToToolIndex(editor::EditMode m) {
    switch (m) {
        case editor::EditMode::Browse:   return kBrowse;
        case editor::EditMode::Tile:     return kTile;
        case editor::EditMode::Foothold: return kFoothold;
        case editor::EditMode::Spawn:    return kSpawn;
        case editor::EditMode::Portal:   return kPortal;
        case editor::EditMode::Object:   return kObject;
    }
    return kBrowse;
}

// --- 사각형 크기 조절 핸들(하얀 네모) ---
// 핸들 8개: 0=좌상 1=상 2=우상 3=우 4=우하 5=하 6=좌하 7=좌. 모서리+변 중앙.
constexpr float kHandleVis = 10.0f; // 화면에 그리는 한 변(픽셀)
constexpr float kHandleHit = 9.0f;  // 잡기 판정 반-크기(픽셀, 시각보다 넉넉히)
constexpr float kRectMinW  = 16.0f; // 사각형 최소 크기(월드 픽셀)

// 화면 사각형 s에서 핸들 h의 중심 위치(화면 좌표).
math::Vector2D HandleCenter(const math::Rect& s, int h) {
    const float midX = s.x + s.w * 0.5f, midY = s.y + s.h * 0.5f;
    switch (h) {
        case 0: return {s.x,       s.y};        case 1: return {midX,      s.y};
        case 2: return {s.Right(), s.y};        case 3: return {s.Right(), midY};
        case 4: return {s.Right(), s.Bottom()}; case 5: return {midX,      s.Bottom()};
        case 6: return {s.x,       s.Bottom()}; case 7: return {s.x,       midY};
    }
    return {};
}
bool CtrlLeft(int h)   { return h == 0 || h == 6 || h == 7; }
bool CtrlRight(int h)  { return h == 2 || h == 3 || h == 4; }
bool CtrlTop(int h)    { return h == 0 || h == 1 || h == 2; }
bool CtrlBottom(int h) { return h == 4 || h == 5 || h == 6; }

// 마우스(화면)가 사각형 s의 어느 핸들 위인가. 없으면 -1.
int HandleAt(const math::Rect& s, math::Vector2D mouse) {
    for (int h = 0; h < 8; ++h) {
        const math::Vector2D c = HandleCenter(s, h);
        if (std::abs(mouse.x - c.x) <= kHandleHit && std::abs(mouse.y - c.y) <= kHandleHit)
            return h;
    }
    return -1;
}

// 핸들 h가 잡은 변(들)을 월드 마우스 위치로 옮긴 새 사각형(자유 비율). 최소 크기 보장.
math::Rect ResizeFree(const math::Rect& w, int h, math::Vector2D m) {
    float l = w.Left(), t = w.Top(), r = w.Right(), b = w.Bottom();
    if (CtrlLeft(h))   l = m.x;
    if (CtrlRight(h))  r = m.x;
    if (CtrlTop(h))    t = m.y;
    if (CtrlBottom(h)) b = m.y;
    if (r - l < kRectMinW) { if (CtrlLeft(h)) l = r - kRectMinW; else r = l + kRectMinW; }
    if (b - t < kRectMinW) { if (CtrlTop(h))  t = b - kRectMinW; else b = t + kRectMinW; }
    return {l, t, r - l, b - t};
}

// 화면(파랑)은 게임 창과 같은 16:9 고정 → 자유 사각형을 비율에 맞춘다.
//  - 세로 변만 잡은 핸들(상/하)은 높이가 폭을 정하고, 그 외(좌/우/모서리)는 폭이 높이를 정한다.
//  - 위쪽 변을 잡았으면 아래를 고정(아래 변을 잡았거나 그 외면 위를 고정)해 자연스럽게 자란다.
math::Rect ConformAspect(const math::Rect& c, int h) {
    if (!CtrlLeft(h) && !CtrlRight(h)) {            // 상/하: 높이가 권위
        const float w = c.h * (kViewW / kViewH);
        return {c.x, c.y, w, c.h};
    }
    const float newH = c.w * (kViewH / kViewW);     // 폭이 권위
    const float y = CtrlTop(h) ? (c.Bottom() - newH) : c.y;
    return {c.x, y, c.w, newH};
}
} // namespace

// 캔버스 = 게임과 1:1 크기, 상단 UI 스트립(kToolStripH) '아래'에 오프셋 배치. UI는 그 스트립에만 두어
// 캔버스(=실제 게임 화면)를 침범하지 않는다 → 편집 화면이 게임 화면과 정확히 같게 보인다(창을 그만큼 키움).
MapEditorScreen::MapEditorScreen() : m_camera(kViewW, kViewH, 0.0f, kToolStripH) {
    m_emptyMenu.Add("새로 만들기");
    m_emptyMenu.Add("열기");
    m_emptyMenu.Add("← 메뉴로");
    m_emptyMenu.Layout(kViewW * 0.5f, 320.0f, 320.0f, 60.0f, 18.0f);

    // 모드 버튼(좌측): 둘러보기 / 격자 / 타일 / 풋홀드 / 스폰 / 포탈 / 배경 / 오브젝트
    m_tools.Add("둘러보기");
    m_tools.Add("격자");
    m_tools.Add("타일");
    m_tools.Add("풋홀드");
    m_tools.Add("스폰");
    m_tools.Add("포탈");
    m_tools.Add("배경");
    m_tools.Add("오브젝트");
    m_tools.LayoutRow(8.0f, 8.0f, 82.0f, 30.0f, 3.0f); // 상단 스트립 1행

    // 파일 버튼(우측): 새로 / 열기 / 저장 / 다른이름 / 메뉴로
    m_files.Add("새로");
    m_files.Add("열기");
    m_files.Add("저장");
    m_files.Add("다른이름");
    m_files.Add("← 메뉴로");
    m_files.LayoutRow(kViewW - (5.0f * 85.0f) - 2.0f, 8.0f, 82.0f, 30.0f, 3.0f); // 상단 스트립 1행(우측)

    // 우측 팔레트: 오브젝트 프리셋 목록(Object 모드일 때만 표시). 패널은 캔버스 영역(스트립 아래)에.
    for (int i = 0; i < core::ObjectPresetCount(); ++i) m_objPalette.Add(core::ObjectPresetAt(i).name);
    m_objPalette.LayoutColumn(kPanelX + 12.0f, kToolStripH + 56.0f, kPanelW - 24.0f, 44.0f, 8.0f);

    // 상단 스트립 2행: 둘러보기 확인용 줌 25%(축소)~200%(확대). 둘러보기·사각형 편집에서만 적용(저장 안 됨).
    m_zoomBar.Setup(25.0f, 200.0f, 100.0f);
    m_zoomBar.Layout(72.0f, 46.0f, 170.0f, 14.0f);

    // 상단 스트립 2행: 이동범위(빨강)/화면(파랑) 지정 토글. 켜고 캔버스를 드래그해 사각형을 그린다(우클릭=해제).
    m_rectTools.Add("이동범위");
    m_rectTools.Add("화면");
    m_rectTools.LayoutRow(256.0f, 42.0f, 96.0f, 26.0f, 4.0f);
}

void MapEditorScreen::StartEditing() {
    world::BuildDefaultMap(m_map);
    m_camera.Follow({640.0f, 360.0f});
    m_editor.emplace(m_map);
}

EditorScene MapEditorScreen::Update(const platform::Input& in, float dt) {
    // --- 빈 상태: 새로 만들기 / 열기 / 메뉴로 ---
    if (!m_editor) {
        if (in.WasPressed(platform::Key::Escape)) return EditorScene::Launcher;
        switch (m_emptyMenu.Update(in)) {
            case kNew0: {
                // 먼저 이름을 정한다 — 취소하면 빈 화면에 머문다(에디터로 진입하지 않음).
                const auto path = platform::SaveFileDialog("새 맵 만들기", "GuideStory 맵",
                                                           "*.gsmap", "gsmap",
                                                           platform::AssetsDir("maps"));
                if (path) { StartEditing(); m_editor->CreateDefault(*path); }
                break;
            }
            case kOpen0: StartEditing(); m_editor->Open(); break; // 곧바로 파일 대화상자
            case kBack0: return EditorScene::Launcher;
            default:     break;
        }
        return EditorScene::Stay;
    }

    // --- 편집 상태 ---
    const bool typing = m_editor->IsTextActive();

    // ESC: 텍스트 입력 중이면 MapEditor가 취소로 소비, 아니면 (변경 확인 후) 런처로 복귀.
    if (in.WasPressed(platform::Key::Escape) && !typing) return ConfirmLeave();

    // 둘러보기 확인용 줌(저장 안 됨) + 사각형 지정 토글. 줌은 둘러보기/사각형 편집에서만 적용(편집 모드는 1:1).
    const bool inspect = (m_editor->Mode() == editor::EditMode::Browse) || (m_editingRect != 0);
    if (!typing) {
        if (inspect) m_zoomBar.Update(in);
        m_camera.SetZoom(inspect ? m_zoomBar.Value() / 100.0f : 1.0f);
        switch (m_rectTools.Update(in)) {
            case 0: m_editingRect = (m_editingRect == 1) ? 0 : 1; break; // 이동범위(빨강) 토글
            case 1: m_editingRect = (m_editingRect == 2) ? 0 : 2; break; // 화면(파랑) 토글
            default: break;
        }
    }

    // 상단 GUI 툴바(타이핑 중에는 비활성). 버튼 클릭은 이 프레임에 소비하고 캔버스로 넘기지 않는다.
    if (!typing) {
        switch (m_tools.Update(in)) {
            case kBrowse:   m_editor->SetMode(editor::EditMode::Browse);   return EditorScene::Stay;
            case kGrid:     m_editor->ToggleGrid();                        return EditorScene::Stay;
            case kTile:     m_editor->SetMode(editor::EditMode::Tile);     return EditorScene::Stay;
            case kFoothold: m_editor->SetMode(editor::EditMode::Foothold); return EditorScene::Stay;
            case kSpawn:    m_editor->SetMode(editor::EditMode::Spawn);    return EditorScene::Stay;
            case kPortal:   m_editor->SetMode(editor::EditMode::Portal);   return EditorScene::Stay;
            case kBackground: {
                const auto p = platform::OpenFileDialog("배경 이미지", "PNG 이미지", "*.png",
                                                        platform::AssetsDir("backgrounds"));
                if (p) m_editor->SetBackground(*p); // 파일명만 맵에 저장(저장 시 .gsmap에 기록)
                return EditorScene::Stay;
            }
            case kObject:   m_editor->SetMode(editor::EditMode::Object);   return EditorScene::Stay;
            default:        break;
        }
        switch (m_files.Update(in)) {
            case kNew:    m_editor->NewMap(); return EditorScene::Stay;
            case kOpen:   m_editor->Open();   return EditorScene::Stay;
            case kSave:   m_editor->Save();   return EditorScene::Stay;
            case kSaveAs: m_editor->SaveAs(); return EditorScene::Stay;
            case kBack:   return ConfirmLeave();
            default:      break;
        }

        // 우측 오브젝트 팔레트(Object 모드에서만). 클릭하면 프리셋 선택 후 소비.
        if (m_editor->Mode() == editor::EditMode::Object) {
            const int sel = m_objPalette.Update(in);
            if (sel >= 0) { m_editor->SelectObject(sel); return EditorScene::Stay; }
        }
    }

    // 포인터가 상단 UI 스트립 또는 우측 팔레트 패널 위면 마우스 편집/드래그를 막는다(클릭 누수 방지).
    const math::Vector2D m = in.MousePos();
    bool overUi = m.y < kToolStripH;
    if (m_editor->Mode() == editor::EditMode::Object && m.x >= kPanelX) overUi = true;
    if (m_zoomBar.Dragging()) overUi = true; // 슬라이더 드래그 중 보호

    // 사각형 편집 모드(이동범위 빨강 / 화면 파랑):
    //  - 좌클릭: 하얀 핸들을 잡아 크기 조절(정밀). 핸들이 아닌 곳을 잡으면 아무 일도 안 한다.
    //  - 우드래그: 처음부터 새 사각형을 그린다(우클릭만 = 0크기 → 해제).
    // 화면(파랑)은 게임 창과 같은 16:9로 폭에 맞춰 높이를 정한다(줌이 폭으로 결정되므로).
    if (m_editingRect != 0) {
        const bool isRed = (m_editingRect == 1);
        const bool has   = isRed ? m_map.HasPlayerBounds() : m_map.HasCameraView();
        const math::Rect cur = isRed ? m_map.PlayerBounds() : m_map.CameraView();
        const math::Vector2D w = m_camera.ScreenToWorld(m);

        if (!overUi) {
            if (in.MouseDown(platform::MouseButton::Right)) {
                // 우드래그: 새 사각형(좌상단~현재). 우클릭만 하면 0크기가 되어 해제된다.
                if (!m_rectDragging) { m_rectDragging = true; m_rectStart = w; }
                const float x = std::min(m_rectStart.x, w.x), y = std::min(m_rectStart.y, w.y);
                const float ww = std::abs(w.x - m_rectStart.x);
                if (isRed) m_map.SetPlayerBounds({x, y, ww, std::abs(w.y - m_rectStart.y)});
                else       m_map.SetCameraView({x, y, ww, ww * kViewH / kViewW}); // 16:9 고정
                m_activeHandle = -1;
            } else {
                m_rectDragging = false;
                // 좌클릭 시작: 핸들을 잡는다.
                if (in.MousePressed(platform::MouseButton::Left))
                    m_activeHandle = has ? HandleAt(m_camera.WorldRectToScreen(cur), m) : -1;
                // 좌드래그: 잡은 핸들로 크기 조절.
                if (in.MouseDown(platform::MouseButton::Left)) {
                    if (m_activeHandle >= 0 && has) {
                        const math::Rect c = ResizeFree(cur, m_activeHandle, w);
                        if (isRed) m_map.SetPlayerBounds(c);
                        else       m_map.SetCameraView(ConformAspect(c, m_activeHandle));
                    }
                } else {
                    m_activeHandle = -1;
                }
            }
        } else {
            m_rectDragging = false;
            m_activeHandle = -1;
        }
        m_editor->Update(in, m_camera, dt, false); // 마우스 편집/패닝 차단(키 패닝은 MapEditor 내부에서 유지)
    } else {
        m_editor->Update(in, m_camera, dt, !overUi);
    }
    return EditorScene::Stay;
}

void MapEditorScreen::Render(platform::IRenderDevice& r) {
    if (!m_editor) {
        r.Clear(kEmptyBg);
        ui::DrawCenteredText(r, "맵 에디터", kViewW * 0.5f, 160.0f, 52.0f, kTitle);
        ui::DrawCenteredText(r, "새로 만들기 또는 열기를 선택하세요",
                             kViewW * 0.5f, 240.0f, 24.0f, kHint);
        m_emptyMenu.Render(r);
        return;
    }

    r.Clear(kSky);

    // 배경 원본 픽셀 크기를 측정해 맵에 심는다(실제 그리기는 공유 WorldRenderer가 담당 → 게임도 동일).
    // → 저장 시 BGSIZE로 기록되고, 배경이 카메라/시각 범위 권위가 된다.
    if (!m_editor->Background().empty()) {
        const platform::TextureId bg = r.LoadTexture(platform::BackgroundPath(m_editor->Background()));
        if (bg != platform::kInvalidTexture) m_editor->SetBackgroundSize(r.TextureSize(bg));
    }

    core::RenderWorld(r, m_camera, m_map); // 배경 → 타일 → 오브젝트 → 풋홀드/포탈
    m_editor->Render(r, m_camera);          // 격자/고스트/마커/상태 오버레이

    // 이동범위(빨강) + 게임 화면(파랑) 가이드. 설정돼 있으면 항상 표시.
    if (m_map.HasPlayerBounds())
        r.DrawRect(m_camera.WorldRectToScreen(m_map.PlayerBounds()), {235, 60, 60, 245});  // 빨강
    if (m_map.HasCameraView())
        r.DrawRect(m_camera.WorldRectToScreen(m_map.CameraView()), {70, 110, 235, 245});   // 파랑

    // 현재 편집 중인 사각형에는 하얀 크기조절 핸들(모서리+변 중앙 8개)을 그린다.
    if (m_editingRect != 0) {
        const bool isRed = (m_editingRect == 1);
        const bool has   = isRed ? m_map.HasPlayerBounds() : m_map.HasCameraView();
        if (has) {
            const math::Rect s = m_camera.WorldRectToScreen(isRed ? m_map.PlayerBounds()
                                                                  : m_map.CameraView());
            for (int h = 0; h < 8; ++h) {
                const math::Vector2D c = HandleCenter(s, h);
                const math::Rect box{c.x - kHandleVis * 0.5f, c.y - kHandleVis * 0.5f,
                                     kHandleVis, kHandleVis};
                r.FillRect(box, {255, 255, 255, 255});
                r.DrawRect(box, {40, 40, 40, 255});
            }
        }
    }

    // 우측 오브젝트 팔레트 패널(Object 모드일 때만). 캔버스 영역(스트립 아래)에 그린다.
    if (m_editor->Mode() == editor::EditMode::Object) {
        r.FillRect({kPanelX, kToolStripH, kPanelW, kViewH}, kPanel);
        ui::DrawCenteredText(r, "오브젝트", kPanelX + kPanelW * 0.5f, kToolStripH + 26.0f, 26.0f, kTitle);
        m_objPalette.SetActive(m_editor->CurrentObject());
        m_objPalette.Render(r);
        ui::DrawCenteredText(r, "클릭해 선택 → 맵에 배치",
                             kPanelX + kPanelW * 0.5f, kWinH - 28.0f, 16.0f, kHint);
    }

    // 상단 UI 스트립(캔버스 위, 별도 영역 — 캔버스를 침범하지 않는다). 2행 구성.
    r.FillRect({0.0f, 0.0f, kViewW, kToolStripH}, kBar);
    m_tools.SetActive(ModeToToolIndex(m_editor->Mode())); // 1행: 모드 버튼(현재 모드 강조)
    m_tools.Render(r);
    m_files.Render(r);                                     // 1행: 파일 버튼(우측)

    const int pct = static_cast<int>(m_zoomBar.Value() + 0.5f); // 2행: 둘러보기 줌 + 사각형 지정
    ui::DrawCenteredText(r, "줌 " + std::to_string(pct) + "%", 36.0f, 53.0f, 15.0f, kTitle);
    m_zoomBar.Render(r);
    m_rectTools.SetActive(m_editingRect - 1); // 0=없음→-1, 1→0(이동범위), 2→1(화면)
    m_rectTools.Render(r);
}

// 닫기/뒤로 전 저장 안 된 변경 확인(네이티브 예/아니오/취소 창). 두 진입점(ESC·창닫기)에서 공유.
namespace {
bool ConfirmAndMaybeSave(editor::MapEditor& ed) {
    switch (platform::AskSaveChanges("GuideStory 맵 에디터",
                                     "변경사항이 있습니다. 저장하시겠습니까?")) {
        case platform::SavePrompt::Save:    ed.Save(); return true;  // 저장 후 진행
        case platform::SavePrompt::Discard: return true;            // 저장 없이 진행
        case platform::SavePrompt::Cancel:  return false;           // 머문다
    }
    return false;
}
} // namespace

EditorScene MapEditorScreen::ConfirmLeave() {
    if (!m_editor || !m_editor->IsDirty()) return EditorScene::Launcher; // 변경 없음 → 바로 복귀
    return ConfirmAndMaybeSave(*m_editor) ? EditorScene::Launcher : EditorScene::Stay;
}

CloseDecision MapEditorScreen::OnCloseRequest() {
    if (!m_editor || !m_editor->IsDirty()) return CloseDecision::Allow; // 변경 없음 → 종료 허용
    return ConfirmAndMaybeSave(*m_editor) ? CloseDecision::Allow : CloseDecision::Cancel;
}

} // namespace gs::app
