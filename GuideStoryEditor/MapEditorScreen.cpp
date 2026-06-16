#include "MapEditorScreen.h"

#include "core/ObjectPalette.h"
#include "core/WorldRenderer.h"
#include "platform/FileDialog.h"
#include "world/MapScaffold.h"

namespace gs::app {

namespace {
constexpr platform::Color kEmptyBg {30, 34, 52, 255};
constexpr platform::Color kSky     {100, 149, 237, 255};
constexpr platform::Color kTitle   {236, 224, 150, 255};
constexpr platform::Color kHint    {150, 160, 180, 255};
constexpr platform::Color kBar     {20, 24, 38, 220}; // 상단 툴바 배경
constexpr platform::Color kPanel   {26, 30, 46, 240}; // 우측 팔레트 패널 배경
constexpr float kBarH   = 40.0f;
constexpr float kPanelW = 210.0f;
constexpr float kPanelX = kViewW - kPanelW; // 우측 패널 좌측 경계

enum EmptyItem { kNew0 = 0, kOpen0, kBack0 };
enum ToolItem  { kBrowse = 0, kGrid, kTile, kFoothold, kSpawn, kPortal, kBackground, kFit, kObject };
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
} // namespace

MapEditorScreen::MapEditorScreen() : m_camera(kViewW, kViewH) {
    m_emptyMenu.Add("새로 만들기");
    m_emptyMenu.Add("열기");
    m_emptyMenu.Add("← 메뉴로");
    m_emptyMenu.Layout(kViewW * 0.5f, 320.0f, 320.0f, 60.0f, 18.0f);

    // 모드 버튼(좌측): 둘러보기 / 격자 / 타일 / 풋홀드 / 스폰 / 포탈 / 배경 / 맞춤 / 오브젝트
    m_tools.Add("둘러보기");
    m_tools.Add("격자");
    m_tools.Add("타일");
    m_tools.Add("풋홀드");
    m_tools.Add("스폰");
    m_tools.Add("포탈");
    m_tools.Add("배경");
    m_tools.Add("맞춤");
    m_tools.Add("오브젝트");
    m_tools.LayoutRow(8.0f, 4.0f, 82.0f, 32.0f, 3.0f);

    // 파일 버튼(우측): 새로 / 열기 / 저장 / 다른이름 / 메뉴로
    m_files.Add("새로");
    m_files.Add("열기");
    m_files.Add("저장");
    m_files.Add("다른이름");
    m_files.Add("← 메뉴로");
    m_files.LayoutRow(kViewW - (5.0f * 85.0f) - 2.0f, 4.0f, 82.0f, 32.0f, 3.0f);

    // 우측 팔레트: 오브젝트 프리셋 목록(Object 모드일 때만 표시).
    for (int i = 0; i < core::ObjectPresetCount(); ++i) m_objPalette.Add(core::ObjectPresetAt(i).name);
    m_objPalette.LayoutColumn(kPanelX + 12.0f, kBarH + 56.0f, kPanelW - 24.0f, 44.0f, 8.0f);
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

    // ESC: 텍스트 입력 중이면 MapEditor가 취소로 소비, 아니면 런처로 복귀.
    if (in.WasPressed(platform::Key::Escape) && !typing) return EditorScene::Launcher;

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
            case kFit: // 맵 격자 크기를 배경 픽셀 크기에 맞춘다(렌더에서 캐시한 m_bgSize 사용)
                m_editor->FitToBackground(static_cast<int>(m_bgSize.x),
                                          static_cast<int>(m_bgSize.y));
                return EditorScene::Stay;
            case kObject:   m_editor->SetMode(editor::EditMode::Object);   return EditorScene::Stay;
            default:        break;
        }
        switch (m_files.Update(in)) {
            case kNew:    m_editor->NewMap(); return EditorScene::Stay;
            case kOpen:   m_editor->Open();   return EditorScene::Stay;
            case kSave:   m_editor->Save();   return EditorScene::Stay;
            case kSaveAs: m_editor->SaveAs(); return EditorScene::Stay;
            case kBack:   return EditorScene::Launcher;
            default:      break;
        }

        // 우측 오브젝트 팔레트(Object 모드에서만). 클릭하면 프리셋 선택 후 소비.
        if (m_editor->Mode() == editor::EditMode::Object) {
            const int sel = m_objPalette.Update(in);
            if (sel >= 0) { m_editor->SelectObject(sel); return EditorScene::Stay; }
        }
    }

    // 포인터가 상단 툴바 또는 우측 팔레트 패널 위면 마우스 편집/드래그를 막는다(클릭 누수 방지).
    const math::Vector2D m = in.MousePos();
    bool overUi = m.y < kBarH;
    if (m_editor->Mode() == editor::EditMode::Object && m.x >= kPanelX) overUi = true;
    m_editor->Update(in, m_camera, dt, !overUi);
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

    // 배경 픽셀 크기를 "맞춤" 버튼용으로 캐시(실제 그리기는 공유 WorldRenderer가 담당 → 게임도 동일).
    m_bgSize = {0.0f, 0.0f};
    if (!m_editor->Background().empty()) {
        const platform::TextureId bg = r.LoadTexture(platform::BackgroundPath(m_editor->Background()));
        if (bg != platform::kInvalidTexture) m_bgSize = r.TextureSize(bg);
    }

    core::RenderWorld(r, m_camera, m_map); // 배경 → 타일 → 오브젝트 → 풋홀드/포탈
    m_editor->Render(r, m_camera);          // 격자/고스트/마커/상태 오버레이

    // 우측 오브젝트 팔레트 패널(Object 모드일 때만). 다른 모드를 고르면 사라진다.
    if (m_editor->Mode() == editor::EditMode::Object) {
        r.FillRect({kPanelX, kBarH, kPanelW, kViewH - kBarH}, kPanel);
        ui::DrawCenteredText(r, "오브젝트", kPanelX + kPanelW * 0.5f, kBarH + 26.0f, 26.0f, kTitle);
        m_objPalette.SetActive(m_editor->CurrentObject());
        m_objPalette.Render(r);
        ui::DrawCenteredText(r, "클릭해 선택 → 맵에 배치",
                             kPanelX + kPanelW * 0.5f, kViewH - 28.0f, 16.0f, kHint);
    }

    // 상단 툴바(월드/패널 위에 그린다). 현재 모드 버튼을 강조한다.
    m_tools.SetActive(ModeToToolIndex(m_editor->Mode()));
    r.FillRect({0.0f, 0.0f, kViewW, kBarH}, kBar);
    m_tools.Render(r);
    m_files.Render(r);
}

} // namespace gs::app
