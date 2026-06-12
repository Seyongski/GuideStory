#pragma once

#include "core/Camera.h"
#include "math/Rect.h"
#include "math/Vector2D.h"
#include "platform/IRenderDevice.h"
#include "world/Map.h"

// 맵(타일 격자 + 풋홀드 선분 + 포탈 + 경계)을 그리는 공통 렌더 헬퍼.
// 에디터·게임 두 실행파일이 동일한 월드 표현을 공유한다(중복 제거).
// 플레이어/에디터 오버레이는 각 앱이 따로 그린다.
namespace gs::core {

// 포탈 표현/충돌 박스(pos = 바닥 중심). 렌더·에디터 히트테스트·게임 겹침판정이 공유.
inline constexpr float kPortalW = 40.0f;
inline constexpr float kPortalH = 60.0f;
inline math::Rect PortalBox(math::Vector2D pos) {
    return {pos.x - kPortalW * 0.5f, pos.y - kPortalH, kPortalW, kPortalH};
}

// 카메라 변환 + 화면 컬링으로 타일·풋홀드·포탈·월드경계를 그린다.
void RenderWorld(platform::IRenderDevice& r, const Camera& cam, const world::Map& map);

} // namespace gs::core
