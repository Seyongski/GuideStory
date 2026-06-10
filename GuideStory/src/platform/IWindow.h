#pragma once

namespace gs::platform {

// ADR-006: 창/이벤트 처리를 인터페이스 뒤로 숨겨 SDL2를 격리한다.
// SDL3 / DirectX / Custom Graphics API로 교체 시 이 인터페이스의 구현체만 새로 작성한다.
class IWindow {
public:
    virtual ~IWindow() = default;

    // 윈도우 이벤트 펌프(입력/종료 등)를 처리한다.
    virtual void PollEvents() = 0;

    // 사용자가 창을 닫으려 했는지 여부.
    virtual bool ShouldClose() const = 0;
};

} // namespace gs::platform
