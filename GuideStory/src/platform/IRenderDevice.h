#pragma once

#include "platform/Color.h"
#include "math/Rect.h"
#include "math/Vector2D.h"

#include <string>

namespace gs::platform {

// 텍스처 핸들. 렌더 디바이스 내부의 캐시 인덱스이며 SDL 타입을 노출하지 않는다(ADR-006).
// kInvalidTexture(-1) = 로드 실패/없음.
using TextureId = int;
inline constexpr TextureId kInvalidTexture = -1;

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

    // --- 텍스처(이미지) ---
    // 경로(UTF-8)의 이미지(PNG 등)를 로드·캐시하고 핸들을 반환한다. 같은 경로는 재사용.
    // 실패하면 kInvalidTexture. (배경/오브젝트 스프라이트용)
    virtual TextureId LoadTexture(const std::string& path) = 0;

    // 텍스처를 화면(픽셀) dst 사각형에 그린다(전체 → dst로 스케일). 핸들이 유효하지 않으면 무시.
    virtual void DrawTexture(TextureId tex, const math::Rect& dst) = 0;

    // 텍스처의 원본 픽셀 크기. 유효하지 않으면 {0,0}. (비율 맞춤·1:1 배치 계산용)
    virtual math::Vector2D TextureSize(TextureId tex) const = 0;

    // 백버퍼를 화면에 표시한다.
    virtual void Present() = 0;
};

} // namespace gs::platform
