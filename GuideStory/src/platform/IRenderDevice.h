#pragma once

#include "platform/Color.h"
#include "math/Rect.h"
#include "math/Vector2D.h"

namespace gs::platform {

// ADR-006: 렌더링을 인터페이스 뒤로 은닉. 게임 로직은 SDL_Renderer를 모른다.
// 모든 좌표는 화면(픽셀) 공간 — 월드→화면 변환은 호출측(Camera)이 담당한다.
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    // 백버퍼를 단색으로 지운다.
    virtual void Clear(const Color& color) = 0;

    // 채워진 사각형 (타일·캐릭터 1차 표현).
    virtual void FillRect(const math::Rect& rect, const Color& color) = 0;

    // 사각형 외곽선 (그리드·디버그).
    virtual void DrawRect(const math::Rect& rect, const Color& color) = 0;

    // 선분 (풋홀드·디버그).
    virtual void DrawLine(const math::Vector2D& a, const math::Vector2D& b, const Color& color) = 0;

    // 백버퍼를 화면에 표시한다.
    virtual void Present() = 0;
};

} // namespace gs::platform
