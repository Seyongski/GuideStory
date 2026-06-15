#include "MapEditorScreen.h"

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
constexpr float kBarH = 40.0f;

enum EmptyItem { kNew0 = 0, kOpen0, kBack0 };
enum ToolItem  { kBrowse = 0, kGrid, kTile, kFoothold, kSpawn, kPortal, kBackground, kFit };
enum FileItem  { kNew = 0, kOpen, kSave, kSaveAs, kBack };

// 현재 편집 모드에 대응하는 모드 툴바 버튼 인덱스(활성 강조용).
int ModeToToolIndex(editor::EditMode m) {
    switch (m) {
        case editor::EditMode::Browse:   return kBrowse;
        case editor::EditMode::Tile:     return kTile;
        case editor::EditMode::Foothold: return kFoothold;
        case editor::EditMode::Spawn:    return kSpawn;
        case editor::EditMode::Portal:   return kPortal;
    }
    return kBrowse;
}
} // namespace

MapEditorScreen::MapEditorScreen() : m_camera(kViewW, kViewH) {
    m_emptyMenu.Add("새로 만들기");
    m_emptyMenu.Add("열기");
    m_emptyMenu.Add("← 메뉴로");
    m_emptyMenu.Layout(kViewW * 0.5f, 320.0f, 320.0f, 60.0f, 18.0f);

    // 모드 버튼(좌측): 둘러보기 / 격자 / 타일 / 풋홀드 / 스폰 / 포탈 / 배경 / 맞춤
    m_tools.Add("둘러보기");
    m_tools.Add("격자");
    m_tools.Add("타일");
    m_tools.Add("풋홀드");
    m_tools.Add("스폰");
    m_tools.Add("포탈");
    m_tools.Add("배경");
    m_tools.Add("맞춤");
    m_tools.LayoutRow(8.0f, 4.0f, 92.0f, 32.0f, 4.0f);

    // 파일 버튼(우측): 새로 / 열기 / 저장 / 다른이름 / 메뉴로
    m_files.Add("새로");
    m_files.Add("열기");
    m_files.Add("저장");
    m_files.Add("다른이름");
    m_files.Add("← 메뉴로");
    m_files.LayoutRow(kViewW - (5.0f * 96.0f) - 4.0f, 4.0f, 92.0f, 32.0f, 4.0f);
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
            case kNew0:  StartEditing(); break;
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
    }

    // 포인터가 툴바 위면 마우스 편집/드래그를 막는다(클릭이 캔버스로 새지 않게).
    const bool overBar = in.MousePos().y < kBarH;
    m_editor->Update(in, m_camera, dt, !overBar);
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

    // 배경 PNG(맵에 설정 시) — 월드 원점에 원본 픽셀 1:1로(왜곡 없음), 타일/풋홀드 뒤에.
    m_bgSize = {0.0f, 0.0f};
    if (!m_editor->Background().empty()) {
        const platform::TextureId bg =
            r.LoadTexture(platform::BackgroundPath(m_editor->Background())); // 경로 캐시 → 호출 저렴
        if (bg != platform::kInvalidTexture) {
            m_bgSize = r.TextureSize(bg); // "맞춤" 버튼이 쓸 배경 픽셀 크기 캐시
            r.DrawTexture(bg, m_camera.WorldRectToScreen({0.0f, 0.0f, m_bgSize.x, m_bgSize.y}));
        }
    }

    core::RenderWorld(r, m_camera, m_map);
    m_editor->Render(r, m_camera);

    // 상단 툴바(월드/오버레이 위에 그린다). 현재 모드 버튼을 강조한다.
    m_tools.SetActive(ModeToToolIndex(m_editor->Mode()));
    r.FillRect({0.0f, 0.0f, kViewW, kBarH}, kBar);
    m_tools.Render(r);
    m_files.Render(r);
}

} // namespace gs::app
