#pragma once

#include "math/Vector2D.h"

#include <string>

// 맵과 맵을 잇는 포탈(이동 지점). 에디터가 배치·연결하고 게임이 이동에 사용한다.
namespace gs::world {

struct Portal {
    int            id = 0;        // 맵 내 고유 번호(대상 포탈 지정에 사용)
    math::Vector2D pos;           // 월드 좌표 — 문의 바닥-중심
    std::string    targetMap;     // 대상 맵 파일명 (빈 문자열 = 미연결)
    int            targetPortal = 0; // 대상 맵의 포탈 id (0 = 대상 맵 스폰으로 이동)
};

} // namespace gs::world
