#include "App.h"

#include "GameScreen.h"
#include "LoginScreen.h"
#include "MainMenuScreen.h"

#include "platform/FileDialog.h" // AssetsDir: 키 설정 저장 경로 해석

#include <chrono>

namespace gs::app {

App::App(platform::IWindow& window, platform::IRenderDevice& renderer)
    : m_window(window),
      m_renderer(renderer),
      m_keySetting(m_bindings),            // 오버레이가 m_bindings를 편집
      m_screen(MakeScreen(SceneId::Login)) // 시작은 로그인창
{
    m_bindings.Load(BindingsPath()); // 저장된 키 설정이 있으면 적용(없으면 기본값 유지)
}

std::string App::BindingsPath() {
    return platform::AssetsDir("config") + "/keybindings.txt";
}

std::unique_ptr<Screen> App::MakeScreen(SceneId id) {
    switch (id) {
        case SceneId::Login:    return std::make_unique<LoginScreen>();
        case SceneId::MainMenu: return std::make_unique<MainMenuScreen>();
        case SceneId::InGame:   return std::make_unique<GameScreen>(m_bindings); // 기본 맵 field01.gsmap
        default:                return std::make_unique<LoginScreen>();
    }
}

void App::Run() {
    using clock = std::chrono::steady_clock;
    auto prev = clock::now();

    while (!m_window.ShouldClose()) {
        m_window.PollEvents();

        const auto now = clock::now();
        float dt = std::chrono::duration<float>(now - prev).count();
        prev = now;
        if (dt > 0.05f) dt = 0.05f; // 스파이크 클램프

        const platform::Input& in = m_window.GetInput();

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
            m_renderer.Present();
            continue;
        }

        if (in.WasPressed(platform::Key::Escape)) break; // ESC = 앱 종료(전역)

        const SceneId next = m_screen->Update(in, dt);
        if (next == SceneId::Quit) break;               // 게임종료
        if (next != SceneId::Stay) {                     // 장면 전환
            m_screen = MakeScreen(next);
        }

        m_screen->Render(m_renderer);
        m_renderer.Present();
    }
}

} // namespace gs::app
