#include "EditorApp.h"

#include "DataEditorScreen.h"
#include "LauncherScreen.h"
#include "MapEditorScreen.h"

namespace gs::app {

EditorApp::EditorApp(platform::IWindow& window, platform::IRenderDevice& renderer)
    : core::HostLoop(window, renderer),
      m_screen(MakeScreen(EditorScene::Launcher)) // 시작은 선택 화면
{
}

std::unique_ptr<EditorScreen> EditorApp::MakeScreen(EditorScene id) {
    switch (id) {
        case EditorScene::MapEditor:     return std::make_unique<MapEditorScreen>();
        case EditorScene::PlayerEditor:  return std::make_unique<DataEditorScreen>("플레이어 에디터");
        case EditorScene::SkillEditor:   return std::make_unique<DataEditorScreen>("스킬 에디터");
        case EditorScene::MonsterEditor: return std::make_unique<DataEditorScreen>("몬스터 에디터");
        case EditorScene::NpcEditor:     return std::make_unique<DataEditorScreen>("NPC 에디터");
        default:                         return std::make_unique<LauncherScreen>();
    }
}

bool EditorApp::Frame(const platform::Input& in, float dt) {
    // 창 닫기(X·강제 종료) 요청: 현재 화면이 저장 안 된 변경을 확인하게 한다.
    // 화면이 Cancel을 돌리면 닫기를 취소(이번 프레임 계속 실행)하고, 아니면 종료한다.
    if (m_window.ShouldClose()) {
        if (m_screen->OnCloseRequest() == CloseDecision::Cancel) m_window.CancelClose();
        else                                                     return false;
    }

    // ESC는 전역 트랩하지 않는다 — 각 화면이 ESC(취소/뒤로/종료)를 스스로 해석한다.
    const EditorScene next = m_screen->Update(in, dt);
    if (next == EditorScene::Quit) return false;     // 애플리케이션 종료
    if (next != EditorScene::Stay) {                  // 화면 전환
        m_screen = MakeScreen(next);
    }

    m_screen->Render(m_renderer);
    return true;
}

} // namespace gs::app
