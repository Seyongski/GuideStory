#include "GameScreen.h"

#include "core/WorldRenderer.h"
#include "platform/FileDialog.h" // MapPath: 맨 파일명을 자산 맵 폴더에 해석
#include "world/MapScaffold.h"

#include <cmath>
#include <cstdio>
#include <exception>
#include <utility>

namespace gs::app {

namespace {
// 카메라 렉 손맛: 응답성 k = kCamBase + kCamPerSpeed·이동속도 (1/초). 클수록 렉↓(빨리 따라잡음).
// 정상상태 렉 거리 = v/k → 1/kCamPerSpeed(≈50px)로 상한 → 빠를수록 자동으로 빨리 붙어 화면 밖 이탈 방지.
// 더블점프 등 빠른 이동은 '속도'가 커서 k가 알아서 오른다 — 상태별 분기 없음(전역 손맛값).
constexpr float kCamBase     = 6.0f;
constexpr float kCamPerSpeed = 0.02f;
} // namespace

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
    if (m_map.HasCameraView()) m_camera.SetZoom(kViewW / m_map.CameraView().w); // 파랑 폭 → 줌
    m_camera.SnapTo(m_player.Position());          // 시작은 플레이어에 스냅
    m_camera.ClampToBounds(m_map.CameraBounds());  // 화면(파랑)이 이동범위(빨강) 안에 머물도록
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

    // 좌우 벽 + 낙사 판정은 화면범위(VR; 없으면 배경 경계)로. 플레이어가 화면 밖으로 못 나간다.
    const math::Rect cb = m_map.CameraBounds();
    m_player.ClampX(cb.Left(), cb.Right());
    // 바닥 아래로 떨어지면 스폰으로 복귀(아랫점프 = 메이플식 스폰 텔레포트).
    if (m_player.Position().y > cb.Bottom() + 200.0f) {
        m_player.SetPosition(m_map.Spawn());
        m_camera.SnapTo(m_player.Position()); // 추락 부활도 순간이동 → 스냅
    }

    // 포탈: 겹친 상태에서 ↑ 키로 대상 맵 이동.
    if (in.WasPressed(platform::Key::Up)) TryEnterPortal();

    UpdateCamera(dt);
    return SceneId::Stay; // 현재는 인게임 유지(ESC는 창에서 앱 종료).
}

void GameScreen::UpdateCamera(float dt) {
    // 줌 = 게임 화면(파랑) 폭을 창 폭으로 채우는 배율. 데드존+렉으로 플레이어를 추적하되 이동범위(빨강)로 클램프.
    // 파랑 = 빨강이면 클램프가 매 프레임 중앙고정 → 카메라 고정. 파랑이 더 작으면 빨강 안에서 줌인된 채 스크롤.
    m_camera.SetZoom(m_map.HasCameraView() ? kViewW / m_map.CameraView().w : 1.0f);

    // 카메라 렉: 응답성 k를 '전체 이동 속도'(수직 포함 — 더블점프도 자연 반영)로만 결정한다.
    const math::Vector2D v = m_player.Velocity();
    const float speed = std::sqrt(v.x * v.x + v.y * v.y);
    const float k = kCamBase + kCamPerSpeed * speed;
    m_camera.FollowLagged(m_player.Position(), dt, k);

    m_camera.ClampToBounds(m_map.CameraBounds());
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
    m_camera.SetZoom(m_map.HasCameraView() ? kViewW / m_map.CameraView().w : 1.0f); // 새 맵 줌
    m_camera.SnapTo(dest); // 포탈 이동은 순간이동 → 스냅(데드존 추적은 다음 프레임부터)
    m_camera.ClampToBounds(m_map.CameraBounds());
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
