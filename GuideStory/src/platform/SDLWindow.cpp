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

    // 텍스트 입력(에디터 필드)용 SDL_TEXTINPUT 이벤트를 켠다. 키 상태 폴링과 공존한다.
    SDL_StartTextInput();
}

SDLWindow::~SDLWindow() {
    // m_window는 unique_ptr이 SDL_DestroyWindow로 자동 해제.
    // 윈도우 파괴 후 SDL 서브시스템 종료. (단일 윈도우 가정 — 다중 윈도우 시 수명 관리 재설계 필요)
    m_window.reset();
    SDL_Quit();
}

void SDLWindow::PollEvents() {
    m_input.BeginFrame(); // 현재 입력을 이전 프레임으로 이월 (엣지 검출용)

    // 이벤트 펌프: 종료 신호 + SDL 내부 키/마우스 상태 갱신.
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            m_shouldClose = true;
        } else if (e.type == SDL_TEXTINPUT) {
            m_input.AppendText(e.text.text); // 타이핑된 문자(레이아웃/Shift 반영)
        }
    }

    // 키보드: 이벤트 누락 없이 hold 상태를 일괄 조회한다.
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    auto k = [&](Key key, SDL_Scancode sc) { m_input.SetKey(key, ks[sc] != 0); };
    k(Key::Left, SDL_SCANCODE_LEFT);   k(Key::Right, SDL_SCANCODE_RIGHT);
    k(Key::Up,   SDL_SCANCODE_UP);     k(Key::Down,  SDL_SCANCODE_DOWN);
    k(Key::Space,SDL_SCANCODE_SPACE);
    k(Key::LCtrl,SDL_SCANCODE_LCTRL);  k(Key::LAlt,  SDL_SCANCODE_LALT);
    k(Key::LShift,SDL_SCANCODE_LSHIFT);
    k(Key::Tab,  SDL_SCANCODE_TAB);    k(Key::Enter, SDL_SCANCODE_RETURN);
    k(Key::Escape,SDL_SCANCODE_ESCAPE); k(Key::Backspace, SDL_SCANCODE_BACKSPACE);
    k(Key::S, SDL_SCANCODE_S); k(Key::L, SDL_SCANCODE_L); k(Key::F, SDL_SCANCODE_F);
    k(Key::E, SDL_SCANCODE_E); k(Key::R, SDL_SCANCODE_R); k(Key::G, SDL_SCANCODE_G);
    k(Key::D, SDL_SCANCODE_D); k(Key::N, SDL_SCANCODE_N);
    k(Key::Num1, SDL_SCANCODE_1); k(Key::Num2, SDL_SCANCODE_2); k(Key::Num3, SDL_SCANCODE_3);
    k(Key::Num4, SDL_SCANCODE_4); k(Key::Num5, SDL_SCANCODE_5); k(Key::Num6, SDL_SCANCODE_6);
    k(Key::Num7, SDL_SCANCODE_7); k(Key::Num8, SDL_SCANCODE_8); k(Key::Num9, SDL_SCANCODE_9);

    // ESC 종료는 각 앱이 판단한다(에디터 텍스트 입력 취소와 충돌 방지). 여기선 키 상태만 노출.
    // 창 닫기 버튼(SDL_QUIT)은 위에서 m_shouldClose로 처리됨.

    // 마우스: 화면 좌표 + 버튼 상태.
    int mx = 0, my = 0;
    const Uint32 mb = SDL_GetMouseState(&mx, &my);
    m_input.SetMousePos(static_cast<float>(mx), static_cast<float>(my));
    m_input.SetMouseButton(MouseButton::Left,   (mb & SDL_BUTTON(SDL_BUTTON_LEFT))   != 0);
    m_input.SetMouseButton(MouseButton::Right,  (mb & SDL_BUTTON(SDL_BUTTON_RIGHT))  != 0);
    m_input.SetMouseButton(MouseButton::Middle, (mb & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0);
}

bool SDLWindow::ShouldClose() const {
    return m_shouldClose;
}

} // namespace gs::platform
