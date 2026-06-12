#pragma once

#include "math/Vector2D.h"

#include <array>
#include <cstddef>
#include <string>

// ADR-006: 입력도 SDL 비의존 논리 타입으로 추상화한다.
// 물리 키(SDL_Scancode)는 SDLWindow에서 이 논리 Key로 1:1 매핑된다.
namespace gs::platform {

enum class Key {
    Left, Right, Up, Down,
    Space, LCtrl, LAlt, LShift,
    Tab, Enter, Escape, Backspace,
    S, L, F, E, R, G, D, N,
    Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Count
};

enum class MouseButton { Left, Right, Middle, Count };

// 한 프레임의 입력 스냅샷. 이전/현재 프레임 비교로 엣지(눌림/뗌)를 제공한다.
// - IsDown:     현재 눌려 있음 (이동·홀드용)
// - WasPressed: 이번 프레임에 눌리기 시작 (점프·토글·클릭용)
// - WasReleased:이번 프레임에 떼짐
class Input {
public:
    bool IsDown(Key k) const      { return m_keyDown[Index(k)]; }
    bool WasPressed(Key k) const  { return m_keyDown[Index(k)] && !m_keyPrev[Index(k)]; }
    bool WasReleased(Key k) const { return !m_keyDown[Index(k)] && m_keyPrev[Index(k)]; }

    bool MouseDown(MouseButton b) const     { return m_mouseDown[Index(b)]; }
    bool MousePressed(MouseButton b) const  { return m_mouseDown[Index(b)] && !m_mousePrev[Index(b)]; }
    bool MouseReleased(MouseButton b) const { return !m_mouseDown[Index(b)] && m_mousePrev[Index(b)]; }

    // 화면(픽셀) 좌표 마우스 위치.
    const math::Vector2D& MousePos() const { return m_mousePos; }

    // 이번 프레임에 타이핑된 문자(UTF-8). 텍스트 입력 필드용(에디터 포탈 대상·맵 크기).
    const std::string& TextInput() const { return m_textInput; }

    // --- 아래는 platform 구현(SDLWindow)이 매 프레임 채운다 ---
    void BeginFrame() {                       // 현재 → 이전으로 이월
        m_keyPrev = m_keyDown;
        m_mousePrev = m_mouseDown;
        m_textInput.clear();                  // 타이핑 버퍼는 프레임마다 초기화
    }
    void SetKey(Key k, bool down)               { m_keyDown[Index(k)] = down; }
    void SetMouseButton(MouseButton b, bool down){ m_mouseDown[Index(b)] = down; }
    void SetMousePos(float x, float y)           { m_mousePos = {x, y}; }
    void AppendText(const char* utf8)            { if (utf8) m_textInput += utf8; }

private:
    static constexpr std::size_t Index(Key k)         { return static_cast<std::size_t>(k); }
    static constexpr std::size_t Index(MouseButton b) { return static_cast<std::size_t>(b); }

    static constexpr std::size_t kKeyCount   = static_cast<std::size_t>(Key::Count);
    static constexpr std::size_t kMouseCount = static_cast<std::size_t>(MouseButton::Count);

    std::array<bool, kKeyCount>   m_keyDown{};
    std::array<bool, kKeyCount>   m_keyPrev{};
    std::array<bool, kMouseCount> m_mouseDown{};
    std::array<bool, kMouseCount> m_mousePrev{};
    math::Vector2D                m_mousePos{};
    std::string                   m_textInput;
};

} // namespace gs::platform
