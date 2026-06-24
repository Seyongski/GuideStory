#pragma once

#include "Screen.h"

#include "core/Camera.h"
#include "core/InputMap.h"
#include "core/PlayerState.h"
#include "physics/PlatformerController.h"
#include "world/Map.h"

#include <string>

namespace gs::app {

// 인게임(플레이) 장면. 에디터가 저장한 맵을 불러와 플레이어가 직접 플레이한다.
// 편집 기능(TAB/MapEditor)은 없다 — 그것은 GuideStoryEditor.exe의 책임.
// 창/타이밍/전환은 App 호스트 루프가 담당한다(이전 GameApp의 Run 루프를 분리).
class GameScreen final : public Screen {
public:
    // bindings: 행동→키 매핑(App이 소유, 키세팅이 편집). 입력은 이 매핑을 통해 질의한다.
    // playerState: App 소유. 맵 진입/포탈 전환 시 마지막 맵을 갱신·저장한다(다음 실행에 복원).
    // mapPath: 맵 파일명(자산 폴더 assets/maps에 해석). 로드 실패 시 기본 맵으로 폴백한다.
    GameScreen(const core::InputMap& bindings, core::PlayerState& playerState,
               std::string mapPath = "field01.gsmap");

    SceneId Update(const platform::Input& in, float dt) override;
    void Render(platform::IRenderDevice& r) override;

private:
    void RenderPlayer(platform::IRenderDevice& r);
    math::Rect PlayerRect() const;
    void TryEnterPortal(); // 겹친 포탈이 있으면 대상 맵으로 이동

    const core::InputMap&         m_bindings;    // App 소유 — 수명은 App이 보장
    core::PlayerState&            m_playerState; // App 소유 — 마지막 맵 기억
    world::Map                    m_map;
    physics::PlatformerController  m_player;
    core::Camera                  m_camera;

    // 플레이어 표현 박스(단색 사각형 1차).
    float m_playerW = 28.0f;
    float m_playerH = 48.0f;
};

} // namespace gs::app
