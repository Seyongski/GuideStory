#include "App.h"

#include "GameScreen.h"
#include "LoginScreen.h"
#include "MainMenuScreen.h"

#include <chrono>

namespace gs::app {

App::App(platform::IWindow& window, platform::IRenderDevice& renderer)
    : m_window(window),
      m_renderer(renderer),
      m_screen(MakeScreen(SceneId::Login)) // 시작은 로그인창
{
}

std::unique_ptr<Screen> App::MakeScreen(SceneId id) {
    switch (id) {
        case SceneId::Login:    return std::make_unique<LoginScreen>();
        case SceneId::MainMenu: return std::make_unique<MainMenuScreen>();
        case SceneId::InGame:   return std::make_unique<GameScreen>(); // 기본 맵 field01.gsmap
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

        const SceneId next = m_screen->Update(m_window.GetInput(), dt);
        if (next == SceneId::Quit) break;               // 게임종료
        if (next != SceneId::Stay) {                     // 장면 전환
            m_screen = MakeScreen(next);
        }

        m_screen->Render(m_renderer);
        m_renderer.Present();
    }
}

} // namespace gs::app
