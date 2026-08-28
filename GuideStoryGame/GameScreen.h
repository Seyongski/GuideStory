#pragma once

#include "Screen.h"

#include "core/Camera.h"
#include "core/ChatOverlay.h"
#include "core/InputMap.h"
#include "core/PlayerState.h"
#include "physics/PlatformerController.h"
#include "world/Map.h"

#include <string>

namespace gs::app {

// 인게임(플레이) 장면. 에디터가 저장한 맵을 불러와 플레이어가 직접 플레이한다.
// 편집 기능(TAB/MapEditor)은 없다 — 그것은 GuideStoryEditor.exe의 책임.
// 창/타이밍/전환은 App 호스트 루프가 담당한다(이전 GameApp의 Run 루프를 분리).
//
// 채팅은 화면(ChatOverlay)과 전송(net::NetClient)을 이 화면이 잇는다 —
// 오버레이는 네트워크를, NetClient는 화면을 서로 모른다.
class GameScreen final : public Screen {
public:
    // net: App 소유. 채팅 송수신에 쓴다(로그인 상태도 여기서 읽는다).
    // bindings: 행동→키 매핑(App이 소유, 키세팅이 편집). 입력은 이 매핑을 통해 질의한다.
    // playerState: App 소유. 맵 진입/포탈 전환 시 마지막 맵을 갱신·저장한다(다음 실행에 복원).
    // mapPath: 맵 파일명(자산 폴더 assets/maps에 해석). 로드 실패 시 기본 맵으로 폴백한다.
    GameScreen(net::NetClient& net, const core::InputMap& bindings,
               core::PlayerState& playerState, std::string mapPath = "field01.gsmap");

    SceneId Update(const platform::Input& in, float dt) override;
    void Render(platform::IRenderDevice& r) override;
    void OnNetEvent(const net::NetEvent& ev) override;

private:
    void RenderPlayer(platform::IRenderDevice& r);
    math::Rect PlayerRect() const;
    void TryEnterPortal(); // 겹친 포탈이 있으면 대상 맵으로 이동

    // 입력줄에서 확정된 한 줄을 해석해 서버로 보낸다("/w 닉 내용" = 귓속말, 그 외 = 전체).
    void SubmitChat(const std::string& line);

    // 카메라를 현재 맵 규칙대로 위치시킨다: 고정 맵이면 월드 중심에 박고(스크롤 없음),
    // 아니면 데드존 + 속도 비례 렉으로 플레이어를 추적한다. 둘 다 경계로 클램프.
    void UpdateCamera(float dt);

    net::NetClient&               m_net;         // App 소유 — 수명은 App이 보장
    const core::InputMap&         m_bindings;    // App 소유 — 수명은 App이 보장
    core::PlayerState&            m_playerState; // App 소유 — 마지막 맵 기억
    world::Map                    m_map;
    physics::PlatformerController  m_player;
    core::Camera                  m_camera;
    core::ChatOverlay             m_chat;

    // 플레이어 표현 박스(단색 사각형 1차).
    float m_playerW = 28.0f;
    float m_playerH = 48.0f;
};

} // namespace gs::app
