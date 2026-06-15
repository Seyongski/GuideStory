#include "EditorApp.h"

#include "DataEditorScreen.h"
#include "LauncherScreen.h"
#include "MapEditorScreen.h"

#include <chrono>

namespace gs::app {

EditorApp::EditorApp(platform::IWindow& window, platform::IRenderDevice& renderer)
    : m_window(window),
      m_renderer(renderer),
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

void EditorApp::Run() {
    using clock = std::chrono::steady_clock;
    auto prev = clock::now();

    while (!m_window.ShouldClose()) {
        m_window.PollEvents();

        const auto now = clock::now();
        float dt = std::chrono::duration<float>(now - prev).count();
        prev = now;
        if (dt > 0.05f) dt = 0.05f; // 스파이크 클램프

        const platform::Input& in = m_window.GetInput();
        // ESC는 전역 트랩하지 않는다 — 각 화면이 ESC(취소/뒤로/종료)를 스스로 해석한다.
        const EditorScene next = m_screen->Update(in, dt);
        if (next == EditorScene::Quit) break;        // 애플리케이션 종료
        if (next != EditorScene::Stay) {              // 화면 전환
            m_screen = MakeScreen(next);
        }

        m_screen->Render(m_renderer);
        m_renderer.Present();
    }
}

} // namespace gs::app
