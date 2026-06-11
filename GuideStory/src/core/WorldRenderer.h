#pragma once

#include "core/Camera.h"
#include "platform/IRenderDevice.h"
#include "world/Map.h"

// 맵(타일 격자 + 풋홀드 선분)을 그리는 공통 렌더 헬퍼.
// 에디터·게임 두 실행파일이 동일한 월드 표현을 공유한다(중복 제거).
// 플레이어/에디터 오버레이는 각 앱이 따로 그린다.
namespace gs::core {

// 카메라 변환 + 화면 컬링으로 타일(단색)과 풋홀드(디버그 초록선)를 그린다.
void RenderWorld(platform::IRenderDevice& r, const Camera& cam, const world::Map& map);

} // namespace gs::core
