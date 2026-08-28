#include "App.h"

#include "GameScreen.h"
#include "LoginScreen.h"
#include "MainMenuScreen.h"

#include "platform/FileDialog.h" // AssetsDir: 설정 파일 경로 해석

namespace gs::app {

App::App(platform::IWindow& window, platform::IRenderDevice& renderer)
    : core::HostLoop(window, renderer),
      m_keySetting(m_bindings),            // 오버레이가 m_bindings를 편집
      m_screen(MakeScreen(SceneId::Login)) // 시작은 로그인창
{
    m_bindings.Load(BindingsPath()); // 저장된 키 설정이 있으면 적용(없으면 기본값 유지)
    m_playerState.Load(PlayerStatePath()); // 마지막 맵 복원(없으면 기본 맵으로 시작)

    // 서버 접속은 워커 스레드가 알아서 한다(재시도 포함). 서버가 꺼져 있어도
    // 게임은 그대로 실행되고, 로그인 화면이 접속 상태를 보여준다.
    m_serverCfg.Load(ServerConfigPath());
    m_net.Start(m_serverCfg.Host(), m_serverCfg.Port());
}

std::string App::BindingsPath() {
    return platform::AssetsDir("config") + "/keybindings.txt";
}

std::string App::PlayerStatePath() {
    return platform::AssetsDir("config") + "/playerstate.txt";
}

std::string App::ServerConfigPath() {
    return platform::AssetsDir("config") + "/server.txt";
}

std::unique_ptr<Screen> App::MakeScreen(SceneId id) {
    switch (id) {
        case SceneId::Login:    return std::make_unique<LoginScreen>(m_net);
        case SceneId::MainMenu: return std::make_unique<MainMenuScreen>(m_net);
        case SceneId::InGame: {
            // 시작 맵 = 마지막으로 있던 맵(저장돼 있으면), 없으면 기본 맵.
            const std::string startMap =
                m_playerState.LastMap().empty() ? "crystalgarden.gsmap" : m_playerState.LastMap();
            return std::make_unique<GameScreen>(m_net, m_bindings, m_playerState, startMap);
        }
        default:                return std::make_unique<LoginScreen>(m_net);
    }
}

bool App::Frame(const platform::Input& in, float dt) {
    // 서버 사건을 현재 장면에 배달한다. 오버레이가 떠 있든 어떤 화면이든 매 프레임 비운다 —
    // 아무도 꺼내지 않으면 워커가 넣은 사건이 무한정 쌓인다(Screen::OnNetEvent 주석).
    net::NetEvent ev;
    while (m_net.Poll(ev)) m_screen->OnNetEvent(ev);

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
        // 로그인창으로 돌아간다 = 로그아웃이다. 저장된 자격을 폐기해야
        // 재연결 시 워커가 옛 계정으로 자동 로그인해버리는 일이 없다.
        if (next == SceneId::Login) m_net.Logout();
        m_screen = MakeScreen(next);
    }

    m_screen->Render(m_renderer);
    return true;
}

} // namespace gs::app
