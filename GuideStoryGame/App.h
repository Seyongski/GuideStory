#pragma once

#include "Screen.h"

#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"

#include <memory>

namespace gs::app {

// 런타임 호스트 루프(합성 루트 다음 단계). 창/렌더러/타이밍을 소유하고
// 현재 장면(Screen)을 갱신·렌더하며, 장면이 요청한 전환을 수행한다.
// 시작 장면은 로그인창이다(로그인 → 메인화면 → 인게임).
// 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다 (ADR-006).
class App {
public:
    App(platform::IWindow& window, platform::IRenderDevice& renderer);

    // 종료(창 닫힘/ESC/게임종료) 전까지 입력 → 갱신 → 렌더를 반복한다.
    void Run();

private:
    static std::unique_ptr<Screen> MakeScreen(SceneId id);

    platform::IWindow&       m_window;
    platform::IRenderDevice& m_renderer;
    std::unique_ptr<Screen>  m_screen;
};

} // namespace gs::app
