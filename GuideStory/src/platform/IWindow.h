#pragma once

#include "platform/Input.h"

namespace gs::platform {

// ADR-006: 창/이벤트 처리를 인터페이스 뒤로 숨겨 SDL2를 격리한다.
// SDL3 / DirectX / Custom Graphics API로 교체 시 이 인터페이스의 구현체만 새로 작성한다.
class IWindow {
public:
    virtual ~IWindow() = default;

    // 윈도우 이벤트 펌프(입력/종료 등)를 처리하고 입력 스냅샷을 갱신한다.
    virtual void PollEvents() = 0;

    // 사용자가 창을 닫으려 했는지 여부.
    virtual bool ShouldClose() const = 0;

    // 닫기 요청을 취소한다(ShouldClose를 다시 false로). 저장 안 된 변경이 있어 닫기를
    // 한 프레임 보류하고 확인창을 띄울 때 호스트가 호출한다.
    virtual void CancelClose() = 0;

    // 직전 PollEvents 시점의 입력 스냅샷.
    virtual const Input& GetInput() const = 0;
};

} // namespace gs::platform
