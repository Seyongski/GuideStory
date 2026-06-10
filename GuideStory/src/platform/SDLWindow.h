#pragma once

#include "platform/IWindow.h"

#include <memory>
#include <string>

struct SDL_Window;

namespace gs::platform {

// IWindow의 SDL2 구현. SDL_Window 핸들을 RAII로 관리한다 (ADR-002).
// SDL 타입은 이 클래스(plus SDLRenderDevice) 내부에만 등장한다 (ADR-006).
class SDLWindow final : public IWindow {
public:
    SDLWindow(const std::string& title, int width, int height);
    ~SDLWindow() override;

    SDLWindow(const SDLWindow&) = delete;
    SDLWindow& operator=(const SDLWindow&) = delete;

    void PollEvents() override;
    bool ShouldClose() const override;

    // 동일 platform 계층 내부(예: SDLRenderDevice)에서만 사용하는 네이티브 핸들 접근자.
    SDL_Window* Native() const { return m_window.get(); }

private:
    // ADR-002: 스마트 포인터 + Custom Deleter로 SDL_DestroyWindow 호출을 캡슐화.
    using WindowPtr = std::unique_ptr<SDL_Window, void (*)(SDL_Window*)>;

    WindowPtr m_window;
    bool m_shouldClose = false;
};

} // namespace gs::platform
