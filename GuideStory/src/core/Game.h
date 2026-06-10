#pragma once

#include "core/Camera.h"
#include "editor/MapEditor.h"
#include "physics/PlatformerController.h"
#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"
#include "world/Map.h"

namespace gs::core {

// 게임 루프 + 씬. 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다 (ADR-006).
class Game {
public:
    Game(platform::IWindow& window, platform::IRenderDevice& renderer);

    // 종료 요청 전까지 입력 → 갱신 → 렌더를 반복한다.
    void Run();

private:
    void Update(float dt);
    void Render();
    void RenderWorld();   // 타일 + 풋홀드 (play/edit 공용)
    void RenderPlayer();
    void BuildDefaultMap();

    platform::IWindow&       m_window;
    platform::IRenderDevice& m_renderer;

    world::Map                    m_map;
    physics::PlatformerController  m_player;
    Camera                        m_camera;
    editor::MapEditor             m_editor;
    bool                          m_editMode = false;

    // 플레이어 표현 박스(단색 사각형 1차).
    float m_playerW = 28.0f;
    float m_playerH = 48.0f;
};

} // namespace gs::core
