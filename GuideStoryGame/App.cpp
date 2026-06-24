#include "App.h"

#include "GameScreen.h"
#include "LoginScreen.h"
#include "MainMenuScreen.h"

#include "platform/FileDialog.h" // AssetsDir: 키 설정 저장 경로 해석

namespace gs::app {

App::App(platform::IWindow& window, platform::IRenderDevice& renderer)
    : core::HostLoop(window, renderer),
      m_keySetting(m_bindings),            // 오버레이가 m_bindings를 편집
      m_screen(MakeScreen(SceneId::Login)) // 시작은 로그인창
{
    m_bindings.Load(BindingsPath()); // 저장된 키 설정이 있으면 적용(없으면 기본값 유지)
    m_playerState.Load(PlayerStatePath()); // 마지막 맵 복원(없으면 기본 맵으로 시작)
}

std::string App::BindingsPath() {
    return platform::AssetsDir("config") + "/keybindings.txt";
}

std::string App::PlayerStatePath() {
    return platform::AssetsDir("config") + "/playerstate.txt";
}

std::unique_ptr<Screen> App::MakeScreen(SceneId id) {
    switch (id) {
        case SceneId::Login:    return std::make_unique<LoginScreen>();
        case SceneId::MainMenu: return std::make_unique<MainMenuScreen>();
        case SceneId::InGame: {
            // 시작 맵 = 마지막으로 있던 맵(저장돼 있으면), 없으면 기본 맵.
            const std::string startMap =
                m_playerState.LastMap().empty() ? "crystalgarden.gsmap" : m_playerState.LastMap();
            return std::make_unique<GameScreen>(m_bindings, m_playerState, startMap);
        }
        default:                return std::make_unique<LoginScreen>();
    }
}

bool App::Frame(const platform::Input& in, float dt) {
    // \ : 키보드 설정 오버레이 토글(어느 화면에서나). 닫기는 변동 시 확인창을 거친다.
    if (in.WasPressed(platform::Key::Backslash)) {
        if (m_keySetting.Visible()) m_keySetting.RequestClose();
        else                        m_keySetting.Open(kViewW, kViewH);
    }

    // 오버레이가 열려 있으면 입력을 독점하고 아래 화면은 일시정지(렌더만).
    if (m_keySetting.Visible()) {
        if (in.WasPressed(platform::Key::Escape)) {
            m_keySetting.RequestClose(); // 오버레이 중 ESC = 닫기 요청(변동 시 확인창)
        } else {
            const auto res = m_keySetting.Update(in);
            if (res.saveRequested) m_bindings.Save(BindingsPath());
        }
        m_screen->Render(m_renderer);
        m_keySetting.Render(m_renderer);
        return true; // Present는 HostLoop이 담당
    }

    if (in.WasPressed(platform::Key::Escape)) return false; // ESC = 앱 종료(전역)

    const SceneId next = m_screen->Update(in, dt);
    if (next == SceneId::Quit) return false;            // 게임종료
    if (next != SceneId::Stay) {                         // 장면 전환
        m_screen = MakeScreen(next);
    }

    m_screen->Render(m_renderer);
    return true;
}

} // namespace gs::app
