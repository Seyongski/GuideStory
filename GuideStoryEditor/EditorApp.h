#pragma once

#include "EditorScreen.h"

#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"

#include <memory>

namespace gs::app {

// 에디터 호스트 루프. 선택 화면(런처)에서 시작해 맵/플레이어/스킬/몬스터/NPC 에디터로 전환한다.
// 각 화면(EditorScreen)이 입력→갱신과 렌더를 담당하고, 창/타이밍/전환은 이 루프가 맡는다.
// 항상 편집용(플레이 경로 없음) — 플레이는 GuideStoryGame.exe의 책임.
// 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다(ADR-006).
class EditorApp {
public:
    EditorApp(platform::IWindow& window, platform::IRenderDevice& renderer);

    // 종료 요청 전까지 입력 → 갱신 → 렌더를 반복한다.
    void Run();

private:
    static std::unique_ptr<EditorScreen> MakeScreen(EditorScene id);

    platform::IWindow&            m_window;
    platform::IRenderDevice&      m_renderer;
    std::unique_ptr<EditorScreen> m_screen;
};

} // namespace gs::app
