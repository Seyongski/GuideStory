#pragma once

#include "platform/Color.h"

namespace gs::platform {

// ADR-006: 렌더링을 인터페이스 뒤로 은닉. 게임 로직은 SDL_Renderer를 모른다.
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    // 백버퍼를 단색으로 지운다.
    virtual void Clear(const Color& color) = 0;

    // 백버퍼를 화면에 표시한다.
    virtual void Present() = 0;
};

} // namespace gs::platform
