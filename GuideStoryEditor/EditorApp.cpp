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
