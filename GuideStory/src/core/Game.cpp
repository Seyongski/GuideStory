#include "core/Game.h"

namespace gs::core {

Game::Game(platform::IWindow& window, platform::IRenderDevice& renderer)
    : m_window(window), m_renderer(renderer) {}

void Game::Run() {
    // 메이플스토리류 하늘색 배경 (placeholder).
    const platform::Color clearColor{100, 149, 237, 255};

    while (!m_window.ShouldClose()) {
        m_window.PollEvents();

        // TODO(1단계): Update(dt) — 이동/점프 물리.

        m_renderer.Clear(clearColor);
        // TODO(1단계): 타일맵/캐릭터 렌더.
        m_renderer.Present();
    }
}

} // namespace gs::core
