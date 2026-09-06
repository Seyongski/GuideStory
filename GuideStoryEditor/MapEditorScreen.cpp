#include "MapEditorScreen.h"

#include "ai/AiConfig.h"
#include "ai/NullShapeGenerator.h"
#include "ai/RemoteShapeGenerator.h"
#include "core/ObjectPalette.h"
#include "core/TilePalette.h"
#include "core/WorldRenderer.h"
#include "platform/FileDialog.h"
#include "world/MapScaffold.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace gs::app {

namespace {
constexpr platform::Color kEmptyBg {30, 34, 52, 255};
constexpr platform::Color kSky     {100, 149, 237, 255};
constexpr platform::Color kTitle   {236, 224, 150, 255};
constexpr platform::Color kHint    {150, 160, 180, 255};
constexpr platform::Color kBar     {20, 24, 38, 220}; // 상단 툴바 배경
constexpr platform::Color kPanel   {26, 30, 46, 240}; // 우측 팔레트 패널 배경
constexpr platform::Color kOk      {120, 220, 140, 255}; // AI 연결됨
constexpr platform::Color kWarn    {235, 180,  90, 255}; // AI 서버 꺼짐
constexpr float kPanelW = 210.0f;
constexpr float kPanelX = kViewW - kPanelW; // 우측 패널 좌측 경계

enum EmptyItem { kNew0 = 0, kOpen0, kBack0 };
enum QuickItem { kqBrowse = 0, kqBg, kqAi };             // 단일 버튼(둘러보기/배경/AI 도형)
enum GridItem  { kg8 = 0, kg16, kg32 };                  // 격자 드롭다운(스냅 칸 크기 px)
enum AddItem   { kaTile = 0, kaFoothold, kaSpawn, kaPortal, kaObject }; // 추가 드롭다운
enum CamItem   { kcView = 0, kcBounds };                 // 카메라 드롭다운(화면/이동범위)
enum FileItem  { kfNew = 0, kfOpen, kfSave, kfSaveAs, kfBack };

// 배치 모드인가(추가 드롭다운 강조 + 우측 팔레트 표시 판단).
bool IsPlacement(editor::EditMode m) {
    return m == editor::EditMode::Tile || m == editor::EditMode::Foothold ||
           m == editor::EditMode::Spawn || m == editor::EditMode::Portal ||
           m == editor::EditMode::Object;
}

// 검색 매칭용 ASCII 소문자화(한글 등 비-ASCII 바이트는 그대로 둔다 → UTF-8 부분일치 안전).
std::string ToLowerAscii(const std::string& s) {
    std::string out = s;
    for (char& c : out) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
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

    // 상단 1행 좌측: 단일 버튼 [둘러보기][배경] + 드롭다운 [격자▼][추가▼][카메라▼].
    m_quick.Add("둘러보기");
    m_quick.Add("배경");
    m_quick.Add("AI 도형");
    m_quick.LayoutRow(8.0f, 8.0f, 82.0f, 30.0f, 3.0f); // 8, 93, 178

    // 격자: 스냅 칸 크기 8/16/32 px. 고르면 격자가 켜지고 그 간격으로 스냅·표시된다.
    m_gridMenu.SetLabel("격자");
    m_gridMenu.Add("8 px");
    m_gridMenu.Add("16 px");
    m_gridMenu.Add("32 px");
    m_gridMenu.SetItemSize(82.0f, 30.0f, 2.0f);
    m_gridMenu.LayoutButton(263.0f, 8.0f, 82.0f, 30.0f);

    m_addMenu.SetLabel("추가");
    m_addMenu.Add("타일");
    m_addMenu.Add("풋홀드");
    m_addMenu.Add("스폰");
    m_addMenu.Add("포탈");
    m_addMenu.Add("오브젝트");
    m_addMenu.SetItemSize(120.0f, 30.0f, 2.0f);
    m_addMenu.LayoutButton(348.0f, 8.0f, 86.0f, 30.0f);

    m_camMenu.SetLabel("카메라");
    m_camMenu.Add("화면");      // kcView   → m_editingRect = 2 (파랑)
    m_camMenu.Add("이동범위");  // kcBounds → m_editingRect = 1 (빨강)
    m_camMenu.SetItemSize(120.0f, 30.0f, 2.0f);
    m_camMenu.LayoutButton(437.0f, 8.0f, 86.0f, 30.0f);

    // 상단 1행 우측: 파일 드롭다운(긴 라벨 → 항목 폭 넓게, 화면 안에 들어오게 좌측 정렬 위치).
    m_fileMenu.SetLabel("파일");
    m_fileMenu.Add("새로 만들기");
    m_fileMenu.Add("열기");
    m_fileMenu.Add("저장하기");
    m_fileMenu.Add("다른 이름으로 저장");
    m_fileMenu.Add("← 메뉴로");
    m_fileMenu.SetItemSize(178.0f, 30.0f, 2.0f);
    m_fileMenu.LayoutButton(kViewW - 190.0f, 8.0f, 178.0f, 30.0f);

    // 상단 2행: 둘러보기 확인용 줌 25%(축소)~200%(확대). 둘러보기·사각형 편집에서만 적용(저장 안 됨).
    m_zoomBar.Setup(25.0f, 200.0f, 100.0f);
    m_zoomBar.Layout(72.0f, 46.0f, 170.0f, 14.0f);

    // 우측 팔레트 검색 칸(타일/오브젝트 공용 위치). 목록은 매 프레임 필터해 채운다.
    m_tileSearch.SetPlaceholder("타일 검색…");
    m_tileSearch.Layout(kPanelX + 12.0f, kToolStripH + 44.0f, kPanelW - 24.0f, 28.0f);
    m_objSearch.SetPlaceholder("오브젝트 검색…");
    m_objSearch.Layout(kPanelX + 12.0f, kToolStripH + 44.0f, kPanelW - 24.0f, 28.0f);

    // 우측 AI 패널: 도형 6종. 라벨 이름은 ai::kShapeLabels 가 단일 출처다
    // (여기서 문자열을 다시 적으면 라벨을 추가할 때 한쪽만 고치게 된다).
    for (int i = 0; i < ai::kShapeLabelCount; ++i)
        m_aiLabels.Add(ai::ShapeLabelName(static_cast<ai::ShapeLabel>(i)));
    m_aiLabels.LayoutColumn(kPanelX + 12.0f, kToolStripH + 60.0f, kPanelW - 24.0f, 36.0f, 6.0f);
}

// AI 생성기를 처음 필요할 때 만든다. assets/config/ai.txt 가 없거나 ENABLED 0 이면
// NullShapeGenerator 로 떨어진다 — **에디터는 AI 없이도 완전히 동작해야 한다**(ADR-015).
void MapEditorScreen::EnsureGenerator() {
    if (m_aiGenerator) return;

    ai::AiConfig cfg;
    cfg.Load(platform::AssetsDir("config") + "/ai.txt");

    if (cfg.Enabled()) {
        auto remote = std::make_unique<ai::RemoteShapeGenerator>();
        remote->Start(cfg.Host(), cfg.Port());
        m_aiGenerator = std::move(remote);
    } else {
        m_aiGenerator = std::make_unique<ai::NullShapeGenerator>();
    }
    m_aiTool.Attach(m_aiGenerator.get());
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
    const math::Vector2D m = in.MousePos();

    // 현재 모드에 맞는 우측 팔레트 검색 칸(타일/오브젝트). ESC·단축키 차단 판단에 쓴다.
    ui::SearchBox* search = ActiveSearch();
    const bool tilePalette = m_editor->Mode() == editor::EditMode::Tile   && m_editingRect == 0 && !m_aiMode;
    const bool objPalette  = m_editor->Mode() == editor::EditMode::Object && m_editingRect == 0 && !m_aiMode;

    // ESC: 검색 포커스 해제 > (MapEditor가 텍스트 취소로 소비) > 변경 확인 후 런처로 복귀.
    if (in.WasPressed(platform::Key::Escape)) {
        if (search && search->Focused()) { search->SetFocused(false); return EditorScene::Stay; }
        // AI 미리보기가 떠 있으면 ESC는 그것부터 취소한다(화면을 떠나기 전에).
        if (m_aiMode && m_aiTool.HasPreview()) { m_aiTool.Reset(); return EditorScene::Stay; }
        if (!typing) return ConfirmLeave();
    }

    // 줌(저장 안 됨, 게임 영향 없음). 모든 모드에서 적용 — 확대한 상태 그대로 풋홀드/타일 등을 배치할 수
    // 있다(모드 전환해도 줌이 풀리지 않는다). 슬라이더 드래그 + 마우스 휠 둘 다로 조정한다.
    if (!typing) {
        if (in.WheelDelta() != 0.0f)                       // 휠 한 칸 = 10% (위=확대, 아래=축소)
            m_zoomBar.SetValue(m_zoomBar.Value() + in.WheelDelta() * 10.0f);
        m_zoomBar.Update(in);
    }
    m_camera.SetZoom(m_zoomBar.Value() / 100.0f);

    // --- 상단 드롭다운(파일/추가/카메라) + 단일 버튼(둘러보기/격자/배경) ---
    bool menuClickConsumed = false;
    if (!typing) {
        const bool wasOpen = m_fileMenu.IsOpen() || m_addMenu.IsOpen() ||
                             m_camMenu.IsOpen() || m_gridMenu.IsOpen();
        const auto rFile = m_fileMenu.Update(in);
        const auto rAdd  = m_addMenu.Update(in);
        const auto rCam  = m_camMenu.Update(in);
        const auto rGrid = m_gridMenu.Update(in);
        // 한 번에 하나만 열림: 방금 연 것만 남기고 나머지 닫는다.
        if (rFile.toggled && m_fileMenu.IsOpen()) { m_addMenu.Close();  m_camMenu.Close(); m_gridMenu.Close(); }
        if (rAdd.toggled  && m_addMenu.IsOpen())  { m_fileMenu.Close(); m_camMenu.Close(); m_gridMenu.Close(); }
        if (rCam.toggled  && m_camMenu.IsOpen())  { m_fileMenu.Close(); m_addMenu.Close(); m_gridMenu.Close(); }
        if (rGrid.toggled && m_gridMenu.IsOpen()) { m_fileMenu.Close(); m_addMenu.Close(); m_camMenu.Close(); }

        switch (rFile.item) { // 파일
            case kfNew:    m_editor->NewMap(); return EditorScene::Stay;
            case kfOpen:   m_editor->Open();   return EditorScene::Stay;
            case kfSave:   m_editor->Save();   return EditorScene::Stay;
            case kfSaveAs: m_editor->SaveAs(); return EditorScene::Stay;
            case kfBack:   return ConfirmLeave();
            default: break;
        }
        switch (rAdd.item) { // 추가(배치 모드) — 선택 시 카메라 편집 해제
            case kaTile:     m_editingRect = 0; m_aiMode = false; m_aiTool.Reset(); m_editor->SetMode(editor::EditMode::Tile);     return EditorScene::Stay;
            case kaFoothold: m_editingRect = 0; m_aiMode = false; m_aiTool.Reset(); m_editor->SetMode(editor::EditMode::Foothold); return EditorScene::Stay;
            case kaSpawn:    m_editingRect = 0; m_aiMode = false; m_aiTool.Reset(); m_editor->SetMode(editor::EditMode::Spawn);    return EditorScene::Stay;
            case kaPortal:   m_editingRect = 0; m_aiMode = false; m_aiTool.Reset(); m_editor->SetMode(editor::EditMode::Portal);   return EditorScene::Stay;
            case kaObject:   m_editingRect = 0; m_aiMode = false; m_aiTool.Reset(); m_editor->SetMode(editor::EditMode::Object);   return EditorScene::Stay;
            default: break;
        }
        switch (rCam.item) { // 카메라(사각형 편집) — 배치 모드는 둘러보기로 내려 캔버스 클릭 누수 방지
            case kcView:   m_editingRect = (m_editingRect == 2) ? 0 : 2; m_aiMode = false; m_aiTool.Reset();
                           m_editor->SetMode(editor::EditMode::Browse); return EditorScene::Stay;
            case kcBounds: m_editingRect = (m_editingRect == 1) ? 0 : 1; m_aiMode = false; m_aiTool.Reset();
                           m_editor->SetMode(editor::EditMode::Browse); return EditorScene::Stay;
            default: break;
        }
        // 격자 칸 크기(px). 고르면 격자가 켜지고, 이미 켜진 '활성' 칸을 다시 누르면 격자를 끈다.
        auto pickGrid = [&](int px) {
            if (m_editor->GridOn() && m_editor->GridStep() == px) m_editor->SetGridOn(false);
            else                                                  m_editor->SetGridStep(px);
        };
        switch (rGrid.item) {
            case kg8:  pickGrid(8);  return EditorScene::Stay;
            case kg16: pickGrid(16); return EditorScene::Stay;
            case kg32: pickGrid(32); return EditorScene::Stay;
            default: break;
        }
        switch (m_quick.Update(in)) { // 단일 버튼
            case kqBrowse: m_editingRect = 0; m_aiMode = false; m_aiTool.Reset();
                           m_editor->SetMode(editor::EditMode::Browse); return EditorScene::Stay;
            case kqAi:
                // 카메라 사각형 편집과 마찬가지로 화면 수준 모드다 — 캔버스 클릭이 새지 않도록
                // MapEditor 는 둘러보기로 내린다.
                m_aiMode = !m_aiMode;
                m_editingRect = 0;
                m_editor->SetMode(editor::EditMode::Browse);
                if (m_aiMode) { EnsureGenerator(); m_aiTool.SetTile(m_editor->CurrentTile()); }
                else          { m_aiTool.Reset(); }
                return EditorScene::Stay;
            case kqBg: {
                const auto p = platform::OpenFileDialog("배경 이미지", "PNG 이미지", "*.png",
                                                        platform::AssetsDir("backgrounds"));
                if (p) m_editor->SetBackground(*p); // 파일명만 맵에 저장(저장 시 .gsmap에 기록)
                return EditorScene::Stay;
            }
            default: break;
        }
        menuClickConsumed = wasOpen || rFile.toggled || rAdd.toggled || rCam.toggled || rGrid.toggled;
    }
    const bool anyMenuOpen = m_fileMenu.IsOpen() || m_addMenu.IsOpen() ||
                             m_camMenu.IsOpen() || m_gridMenu.IsOpen();

    // --- 우측 팔레트(타일/오브젝트): 검색 + 필터된 목록 ---
    if ((tilePalette || objPalette) && !typing) {
        if (search && !anyMenuOpen && !menuClickConsumed) search->Update(in);
        BuildPalette(tilePalette, search ? search->Text() : std::string());
        if (!anyMenuOpen && !menuClickConsumed) {
            const int sel = m_paletteList.Update(in);
            if (sel >= 0 && sel < static_cast<int>(m_paletteIds.size())) {
                if (tilePalette) m_editor->SetCurrentTile(static_cast<world::TileId>(m_paletteIds[sel]));
                else             m_editor->SelectObject(m_paletteIds[sel]);
                return EditorScene::Stay;
            }
        }
    }

    // --- 우측 AI 패널: 도형 라벨 선택 ---
    if (m_aiMode && !typing && !anyMenuOpen && !menuClickConsumed) {
        const int pick = m_aiLabels.Update(in);
        if (pick >= 0 && pick < ai::kShapeLabelCount) {
            m_aiTool.SetLabel(static_cast<ai::ShapeLabel>(pick));
            return EditorScene::Stay;
        }
    }

    // 검색 칸에 포커스가 있으면 타이핑 중 — 캔버스 편집/단축키/패닝을 모두 멈춘다(렌더는 계속).
    if (search && search->Focused()) return EditorScene::Stay;

    // 포인터가 상단 스트립/열린 드롭다운/우측 팔레트 위면 캔버스 편집을 막는다(클릭 누수 방지).
    bool overUi = m.y < kToolStripH;
    if ((tilePalette || objPalette || m_aiMode) && m.x >= kPanelX) overUi = true;
    if (anyMenuOpen || menuClickConsumed) overUi = true;
    if (m_zoomBar.Dragging()) overUi = true; // 슬라이더 드래그 중 보호

    // 사각형 편집 모드(이동범위 빨강 / 화면 파랑):
    //  - 좌클릭: 하얀 핸들을 잡아 크기 조절(정밀). 핸들이 아닌 곳을 잡으면 아무 일도 안 한다.
    //  - 우드래그: 처음부터 새 사각형을 그린다(우클릭만 = 0크기 → 해제).
    // 화면(파랑)은 게임 창과 같은 16:9로 폭에 맞춰 높이를 정한다(줌이 폭으로 결정되므로).
    if (m_aiMode) {
        // AI 도구가 캔버스 드래그(영역 선택)와 Enter/R/Esc 를 가져간다.
        // MapEditor 는 마우스 편집 없이 키 패닝만 유지한다(카메라 사각형 편집과 같은 처리).
        m_aiTool.SetTile(m_editor->CurrentTile());
        m_aiTool.Update(in, m_camera, m_map, !overUi);
        m_editor->Update(in, m_camera, dt, false);
        return EditorScene::Stay;
    }

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
    if (m_aiMode) m_aiTool.Render(r, m_camera, m_map); // AI 선택 사각형 + 고스트 + 예상 풋홀드

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

    // 우측 팔레트 패널(타일/오브젝트 모드일 때). 검색 + 필터된 종류 목록. 캔버스 영역(스트립 아래)에.
    const bool tilePalette = m_editor->Mode() == editor::EditMode::Tile   && m_editingRect == 0 && !m_aiMode;
    const bool objPalette  = m_editor->Mode() == editor::EditMode::Object && m_editingRect == 0 && !m_aiMode;
    if (m_aiMode) {
        r.FillRect({kPanelX, kToolStripH, kPanelW, kViewH}, kPanel);
        const float cx = kPanelX + kPanelW * 0.5f;
        ui::DrawCenteredText(r, "AI 도형", cx, kToolStripH + 22.0f, 24.0f, kTitle);

        m_aiLabels.SetActive(static_cast<int>(m_aiTool.Label()));
        m_aiLabels.Render(r);

        // 생성기 상태 — 어느 경로인지, **지금 붙어 있는지**를 화면에서 바로 보여준다.
        // 생성을 시도해야만 "서버 꺼짐"을 알 수 있으면 늦다(처음 쓸 때 그대로 막힌다).
        const float infoY = kToolStripH + 60.0f + ai::kShapeLabelCount * 42.0f + 12.0f;
        const bool  ready = m_aiGenerator && m_aiGenerator->Ready();
        const char* gname = m_aiGenerator ? m_aiGenerator->Name() : "none";

        ui::DrawCenteredText(r, gname, cx, infoY, 13.0f, kHint);
        ui::DrawCenteredText(r, ready ? "● 연결됨" : "● 서버 꺼짐",
                             cx, infoY + 20.0f, 15.0f, ready ? kOk : kWarn);

        if (ready) {
            if (m_aiTool.LastRoundTripMs() > 0.0) {
                const int rt = static_cast<int>(m_aiTool.LastRoundTripMs() + 0.5);
                ui::DrawCenteredText(r, "왕복 " + std::to_string(rt) + " ms",
                                     cx, infoY + 42.0f, 13.0f, kHint);
            }
        } else {
            // 무엇을 해야 하는지 화면에서 알려준다 — 문서를 찾아보게 만들지 않는다.
            ui::DrawCenteredText(r, "추론 서버를 먼저 실행하세요", cx, infoY + 46.0f, 14.0f, kHint);
            ui::DrawCenteredText(r, "GuideStoryAI 폴더에서", cx, infoY + 68.0f, 12.0f, kHint);
            ui::DrawCenteredText(r, "python serve/ai_server.py --stub", cx, infoY + 86.0f, 12.0f, kHint);
            ui::DrawCenteredText(r, "(켜면 2초 안에 자동 연결)", cx, infoY + 108.0f, 12.0f, kHint);
        }

        // 상태 한 줄은 캔버스 하단 가운데 — 시선이 도형에 있을 때 같이 읽힌다.
        ui::DrawCenteredText(r, m_aiTool.Status(), kViewW * 0.5f, kWinH - 48.0f, 17.0f, kTitle);
        ui::DrawCenteredText(r, "드래그로 영역 선택 → Enter 확정 / R 재생성 / Esc 취소",
                             kViewW * 0.5f, kWinH - 22.0f, 15.0f, kHint);
    }
    if (tilePalette || objPalette) {
        r.FillRect({kPanelX, kToolStripH, kPanelW, kViewH}, kPanel);
        const float cx = kPanelX + kPanelW * 0.5f;
        ui::DrawCenteredText(r, tilePalette ? "타일" : "오브젝트", cx, kToolStripH + 22.0f, 24.0f, kTitle);

        ui::SearchBox& sb = tilePalette ? m_tileSearch : m_objSearch;
        sb.Render(r);

        // 목록을 현재 검색어로 다시 채우고(렌더 패스 독립), 현재 선택 항목을 강조한다.
        BuildPalette(tilePalette, sb.Text());
        const int current = tilePalette ? static_cast<int>(m_editor->CurrentTile())
                                        : m_editor->CurrentObject();
        int activeRow = -1;
        for (int i = 0; i < static_cast<int>(m_paletteIds.size()); ++i)
            if (m_paletteIds[i] == current) { activeRow = i; break; }
        m_paletteList.SetActive(activeRow);
        m_paletteList.Render(r);

        ui::DrawCenteredText(r, "클릭해 선택 → 캔버스에 배치", cx, kWinH - 24.0f, 15.0f, kHint);
    }

    // 상단 UI 스트립(캔버스 위, 별도 영역 — 캔버스를 침범하지 않는다). 2행 구성.
    r.FillRect({0.0f, 0.0f, kViewW, kToolStripH}, kBar);

    // 1행: 단일 버튼 + 드롭다운 헤더. 현재 상태에 따라 강조(active).
    m_quick.SetActive(m_aiMode ? kqAi
                      : (m_editor->Mode() == editor::EditMode::Browse && m_editingRect == 0) ? kqBrowse
                      : -1);
    m_quick.Render(r);
    // 격자: 헤더에 현재 칸 크기를 보여주고(예 "격자 16"), 켜져 있으면 헤더와 해당 칸 항목을 밝게 강조한다.
    const int gstep = m_editor->GridStep();
    m_gridMenu.SetLabel("격자 " + std::to_string(gstep));
    m_gridMenu.SetActive(m_editor->GridOn());
    m_gridMenu.SetActiveItem(m_editor->GridOn() ? (gstep == 8 ? kg8 : gstep == 16 ? kg16 : kg32) : -1);
    m_addMenu.SetActive(!m_aiMode && m_editingRect == 0 && IsPlacement(m_editor->Mode()));
    m_camMenu.SetActive(m_editingRect != 0);
    m_gridMenu.RenderHeader(r);
    m_addMenu.RenderHeader(r);
    m_camMenu.RenderHeader(r);
    m_fileMenu.RenderHeader(r);

    // 2행: 둘러보기 줌.
    const int pct = static_cast<int>(m_zoomBar.Value() + 0.5f);
    ui::DrawCenteredText(r, "줌 " + std::to_string(pct) + "%", 36.0f, 53.0f, 15.0f, kTitle);
    m_zoomBar.Render(r);

    // 펼쳐진 드롭다운 목록은 맨 위에 덧그린다(캔버스/팔레트/스트립을 덮는다).
    m_fileMenu.RenderPopup(r);
    m_addMenu.RenderPopup(r);
    m_camMenu.RenderPopup(r);
    m_gridMenu.RenderPopup(r);
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

ui::SearchBox* MapEditorScreen::ActiveSearch() {
    if (!m_editor || m_editingRect != 0) return nullptr;
    if (m_editor->Mode() == editor::EditMode::Tile)   return &m_tileSearch;
    if (m_editor->Mode() == editor::EditMode::Object) return &m_objSearch;
    return nullptr;
}

void MapEditorScreen::BuildPalette(bool tiles, const std::string& query) {
    const int kind = tiles ? 0 : 1;
    if (kind == m_paletteKind && query == m_paletteQuery) return; // 종류·검색어 그대로면 캐시 사용
    m_paletteKind = kind;
    m_paletteQuery = query;

    m_paletteList.Clear();
    m_paletteIds.clear();
    const std::string q = ToLowerAscii(query);
    auto matches = [&](const std::string& name) {
        return q.empty() || ToLowerAscii(name).find(q) != std::string::npos;
    };
    if (tiles) {
        for (int i = 0; i < core::TilePresetCount(); ++i) {
            const core::TilePreset& t = core::TilePresetAt(i);
            if (matches(t.name)) { m_paletteList.Add(t.name); m_paletteIds.push_back(t.id); }
        }
    } else {
        for (int i = 0; i < core::ObjectPresetCount(); ++i) {
            if (matches(core::ObjectPresetAt(i).name)) {
                m_paletteList.Add(core::ObjectPresetAt(i).name);
                m_paletteIds.push_back(i);
            }
        }
    }
    m_paletteList.LayoutColumn(kPanelX + 12.0f, kToolStripH + 84.0f, kPanelW - 24.0f, 36.0f, 6.0f);
}

EditorScene MapEditorScreen::ConfirmLeave() {
    if (!m_editor || !m_editor->IsDirty()) return EditorScene::Launcher; // 변경 없음 → 바로 복귀
    return ConfirmAndMaybeSave(*m_editor) ? EditorScene::Launcher : EditorScene::Stay;
}

CloseDecision MapEditorScreen::OnCloseRequest() {
    if (!m_editor || !m_editor->IsDirty()) return CloseDecision::Allow; // 변경 없음 → 종료 허용
    return ConfirmAndMaybeSave(*m_editor) ? CloseDecision::Allow : CloseDecision::Cancel;
}

} // namespace gs::app
