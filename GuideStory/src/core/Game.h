#pragma once

#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"

namespace gs::core {

// 게임 루프. 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다 (ADR-006).
class Game {
public:
    Game(platform::IWindow& window, platform::IRenderDevice& renderer);

    // 종료 요청 전까지 입력 → 갱신 → 렌더를 반복한다.
    void Run();

private:
    platform::IWindow& m_window;
    platform::IRenderDevice& m_renderer;
};

} // namespace gs::core
