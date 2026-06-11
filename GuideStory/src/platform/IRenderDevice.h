#pragma once

#include "platform/Color.h"
#include "math/Rect.h"
#include "math/Vector2D.h"

#include <string>

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

    // UTF-8 텍스트 (메뉴·HUD). topLeft은 화면(픽셀) 좌상단, pixelHeight는 글자 높이(px).
    // 폰트를 사용할 수 없으면 아무것도 그리지 않는다(견고성 — 게임은 계속 동작).
    virtual void DrawText(const std::string& utf8, const math::Vector2D& topLeft,
                          float pixelHeight, const Color& color) = 0;

    // 주어진 픽셀 높이로 렌더했을 때의 텍스트 크기(px). 폰트 미가용 시 {0,0}.
    // 가운데 정렬 등 레이아웃 계산에 쓴다.
    virtual math::Vector2D MeasureText(const std::string& utf8, float pixelHeight) const = 0;

    // 백버퍼를 화면에 표시한다.
    virtual void Present() = 0;
};

} // namespace gs::platform
