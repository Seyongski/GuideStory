#include "GameScreen.h"

#include "core/WorldRenderer.h"
#include "platform/FileDialog.h" // MapPath: 맨 파일명을 자산 맵 폴더에 해석
#include "world/MapScaffold.h"

#include <cstdio>
#include <exception>
#include <utility>

namespace gs::app {

GameScreen::GameScreen(const core::InputMap& bindings, core::PlayerState& playerState,
                       std::string mapPath)
    : m_bindings(bindings), m_playerState(playerState), m_camera(kViewW, kViewH) {
    // 에디터가 저장한 맵을 로드한다(자산 폴더 assets/maps에서 해석).
    // 실패하면 기본 맵으로 폴백하고 계속 실행한다.
    try {
        m_map.Load(platform::MapPath(mapPath));
        m_playerState.SetLastMap(mapPath); // 진입한 맵 기억(다음 실행 시 이 맵 스폰에서 시작)
    } catch (const std::exception& e) {
        std::fprintf(stderr, "맵 로드 실패(%s) — 기본 맵으로 폴백: %s\n",
                     mapPath.c_str(), e.what());
        world::BuildDefaultMap(m_map);
    }

    m_player.SetPosition(m_map.Spawn());
    m_camera.SnapTo(m_player.Position()); // 시작은 플레이어에 맞춰 스냅(데드존 무시)
    m_camera.ClampToBounds(m_map.WorldBounds());
}

SceneId GameScreen::Update(const platform::Input& in, float dt) {
    // 입력 → 이동 의도 번역.
    physics::MoveIntent intent;
    if (in.IsDown(platform::Key::Left))  intent.moveX -= 1.0f;
    if (in.IsDown(platform::Key::Right)) intent.moveX += 1.0f;

    const bool jumpEdge = m_bindings.WasPressed(in, core::Action::Jump); // 기본 Alt, 키세팅으로 변경 가능
    if (jumpEdge && in.IsDown(platform::Key::Down)) {
        intent.dropDown = true; // ↓ + 점프 = 드롭다운
    } else if (jumpEdge) {
        intent.jump = true;
    }

    m_player.Update(intent, m_map.Footholds(), dt);

    // 월드 경계: 좌우 벽으로 가두고, 바닥 아래로 떨어지면 스폰으로 복귀.
    const math::Rect wb = m_map.WorldBounds();
    m_player.ClampX(wb.Left(), wb.Right());
    if (m_player.Position().y > wb.Bottom() + 200.0f) {
        m_player.SetPosition(m_map.Spawn());
        m_camera.SnapTo(m_player.Position()); // 추락 부활도 순간이동 → 스냅
    }

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
        m_map.Load(platform::MapPath(targetMap)); // 실패 시 예외 → m_map 불변(강한 보장)
        m_playerState.SetLastMap(targetMap); // 전환한 맵 기억(다음 실행 시 이 맵에서 시작)
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
    m_camera.SnapTo(dest); // 포탈 이동은 순간이동 → 스냅(데드존 추적은 다음 프레임부터)
    m_camera.ClampToBounds(m_map.WorldBounds());
}

void GameScreen::Render(platform::IRenderDevice& r) {
    r.Clear({100, 149, 237, 255}); // 하늘색(배경 PNG가 못 덮는 가장자리 폴백)

    // 배경 PNG → 타일 → 오브젝트 → 풋홀드/포탈은 공유 WorldRenderer가 단일 출처로 그린다(에디터와 동일).
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
