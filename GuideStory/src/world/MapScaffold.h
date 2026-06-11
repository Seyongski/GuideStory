#pragma once

#include "world/Map.h"

// 기본(스캐폴드) 맵 빌더. 에디터의 시작 편집 기준 맵이자,
// 게임이 맵 파일 로드에 실패했을 때의 폴백으로 쓰인다.
// 추후 맵 파일이 정착되면 이 코드 의존은 줄어든다.
namespace gs::world {

void BuildDefaultMap(Map& map);

} // namespace gs::world
