#include "GameScreen.h"

#include "core/WorldRenderer.h"
#include "world/MapScaffold.h"

#include <cstdio>
#include <exception>
#include <utility>

namespace gs::app {

GameScreen::GameScreen(std::string mapPath)
    : m_camera(kViewW, kViewH) {
    // 에디터가 저장한 맵을 로드한다. 실패하면 기본 맵으로 폴백하고 계속 실행한다.
    try {
        m_map.Load(mapPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "맵 로드 실패(%s) — 기본 맵으로 폴백: %s\n",
                     mapPath.c_str(), e.what());
        world::BuildDefaultMap(m_map);
    }

    m_player.SetPosition({200.0f, 560.0f}); // 바닥(y=640) 위 — 떨어지며 착지
    m_camera.Follow(m_player.Position());
}

SceneId GameScreen::Update(const platform::Input& in, float dt) {
    // 입력 → 이동 의도 번역.
    physics::MoveIntent intent;
    if (in.IsDown(platform::Key::Left))  intent.moveX -= 1.0f;
    if (in.IsDown(platform::Key::Right)) intent.moveX += 1.0f;

    const bool jumpEdge = in.WasPressed(platform::Key::Space);
    if (jumpEdge && in.IsDown(platform::Key::Down)) {
        intent.dropDown = true; // ↓ + 점프 = 드롭다운
    } else if (jumpEdge) {
        intent.jump = true;
    }

    m_player.Update(intent, m_map.Footholds(), dt);
    m_camera.Follow(m_player.Position());
    return SceneId::Stay; // 현재는 인게임 유지(ESC는 창에서 앱 종료).
}

void GameScreen::Render(platform::IRenderDevice& r) {
    r.Clear({100, 149, 237, 255}); // 하늘색
    core::RenderWorld(r, m_camera, m_map);
    RenderPlayer(r);
}

void GameScreen::RenderPlayer(platform::IRenderDevice& r) {
    const math::Vector2D feet = m_player.Position();
    const math::Rect box{feet.x - m_playerW * 0.5f, feet.y - m_playerH, m_playerW, m_playerH};
    const math::Rect sr = m_camera.WorldRectToScreen(box);
    const platform::Color c = m_player.Grounded()
        ? platform::Color{230, 80, 80, 255}    // 지상 = 빨강
        : platform::Color{230, 160, 60, 255};   // 공중 = 주황
    r.FillRect(sr, c);
}

} // namespace gs::app
