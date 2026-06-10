#include "platform/SDLWindow.h"

#include <SDL.h>

#include <stdexcept>
#include <string>

namespace gs::platform {

SDLWindow::SDLWindow(const std::string& title, int width, int height)
    : m_window(nullptr, &SDL_DestroyWindow) // ADR-002: Custom Deleter
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error(std::string("SDL_Init 실패: ") + SDL_GetError());
    }

    SDL_Window* raw = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN);

    if (raw == nullptr) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow 실패: ") + SDL_GetError());
    }

    // 소유권을 스마트 포인터로 이전 — 이 시점 이후 누수/이중 해제는 RAII가 막는다.
    m_window.reset(raw);
}

SDLWindow::~SDLWindow() {
    // m_window는 unique_ptr이 SDL_DestroyWindow로 자동 해제.
    // 윈도우 파괴 후 SDL 서브시스템 종료. (단일 윈도우 가정 — 다중 윈도우 시 수명 관리 재설계 필요)
    m_window.reset();
    SDL_Quit();
}

void SDLWindow::PollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            m_shouldClose = true;
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            m_shouldClose = true;
        }
    }
}

bool SDLWindow::ShouldClose() const {
    return m_shouldClose;
}

} // namespace gs::platform
