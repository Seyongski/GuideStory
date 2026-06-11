#pragma once

#include "core/Camera.h"
#include "editor/MapEditor.h"
#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"
#include "world/Map.h"

namespace gs::app {

// 에디터 호스트 루프. 맵(타일·풋홀드)을 편집하고 파일로 저장한다.
// 추후 몬스터/캐릭터 스킬 등 "변경 가능한 리소스"의 배치 편집도 이 실행파일이 담당한다.
// 항상 편집 모드(플레이 경로 없음) — 플레이는 GuideStoryGame.exe의 책임.
// 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다 (ADR-006).
class EditorApp {
public:
    EditorApp(platform::IWindow& window, platform::IRenderDevice& renderer);

    // 종료 요청 전까지 입력 → 갱신 → 렌더를 반복한다.
    void Run();

private:
    void Update(float dt);
    void Render();

    platform::IWindow&       m_window;
    platform::IRenderDevice& m_renderer;

    world::Map         m_map;
    core::Camera       m_camera;
    editor::MapEditor  m_editor;
};

} // namespace gs::app
