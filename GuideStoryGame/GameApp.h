#pragma once

#include "core/Camera.h"
#include "physics/PlatformerController.h"
#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"
#include "world/Map.h"

#include <string>

namespace gs::app {

// 런타임(플레이 전용) 호스트 루프. 에디터가 저장한 맵 파일을 불러와 플레이어가 직접 플레이한다.
// 편집 기능(TAB/MapEditor)은 없다 — 그것은 GuideStoryEditor.exe의 책임.
// 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다 (ADR-006).
class GameApp {
public:
    // mapPath: 실행 폴더 기준 맵 파일. 로드 실패 시 기본 맵으로 폴백한다.
    GameApp(platform::IWindow& window, platform::IRenderDevice& renderer,
            std::string mapPath = "field01.gsmap");

    // 종료 요청 전까지 입력 → 갱신 → 렌더를 반복한다.
    void Run();

private:
    void Update(float dt);
    void Render();
    void RenderPlayer();

    platform::IWindow&       m_window;
    platform::IRenderDevice& m_renderer;

    world::Map                    m_map;
    physics::PlatformerController  m_player;
    core::Camera                  m_camera;

    // 플레이어 표현 박스(단색 사각형 1차).
    float m_playerW = 28.0f;
    float m_playerH = 48.0f;
};

} // namespace gs::app
