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

    m_player.SetPosition(m_map.Spawn());
    m_camera.Follow(m_player.Position());
    m_camera.ClampToBounds(m_map.WorldBounds());
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

    // 월드 경계: 좌우 벽으로 가두고, 바닥 아래로 떨어지면 스폰으로 복귀.
    const math::Rect wb = m_map.WorldBounds();
    m_player.ClampX(wb.Left(), wb.Right());
    if (m_player.Position().y > wb.Bottom() + 200.0f) m_player.SetPosition(m_map.Spawn());

    // 포탈: 겹친 상태에서 ↑ 키로 대상 맵 이동.
    if (in.WasPressed(platform::Key::Up)) TryEnterPortal();

    m_camera.Follow(m_player.Position());
    m_camera.ClampToBounds(m_map.WorldBounds());
    return SceneId::Stay; // 현재는 인게임 유지(ESC는 창에서 앱 종료).
}

math::Rect GameScreen::PlayerRect() const {
    const math::Vector2D feet = m_player.Position();
    return {feet.x - m_playerW * 0.5f, feet.y - m_playerH, m_playerW, m_playerH};
}

void GameScreen::TryEnterPortal() {
    // 겹친 연결 포탈을 찾아 대상 정보를 복사한다(Load가 m_map을 교체하므로 참조 보관 금지).
    const math::Rect pr = PlayerRect();
    std::string targetMap;
    int targetPortal = 0;
    for (const auto& p : m_map.Portals()) {
        if (!p.targetMap.empty() && pr.Intersects(core::PortalBox(p.pos))) {
            targetMap = p.targetMap;
            targetPortal = p.targetPortal;
            break;
        }
    }
    if (targetMap.empty()) return;

    try {
        m_map.Load(targetMap); // 실패 시 예외 → m_map 불변(강한 보장)
    } catch (const std::exception& e) {
        std::fprintf(stderr, "포탈 대상 맵 로드 실패(%s): %s\n", targetMap.c_str(), e.what());
        return;
    }

    // 대상 포탈 id가 있으면 그 위치로, 없으면 대상 맵 스폰으로.
    math::Vector2D dest = m_map.Spawn();
    if (targetPortal != 0) {
        for (const auto& p : m_map.Portals())
            if (p.id == targetPortal) { dest = p.pos; break; }
    }
    m_player.SetPosition(dest);
    m_camera.Follow(dest);
    m_camera.ClampToBounds(m_map.WorldBounds());
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
