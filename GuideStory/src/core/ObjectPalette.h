#pragma once

#include "platform/Color.h"

// 배치 오브젝트(건물 등)의 단색 프리셋 — 스프라이트 도입 전 1차 표현.
// 크기는 타일 단위(격자 정렬). 추후 preset이 스프라이트(아틀라스 srcRect)로 확장된다.
// 에디터(팔레트·고스트)와 WorldRenderer(실제 렌더)가 공유한다.
namespace gs::core {

struct ObjectPreset {
    const char*     name;   // 팔레트 라벨
    int             wTiles; // 가로 칸 수
    int             hTiles; // 세로 칸 수
    platform::Color color;  // 단색 채움
};

// 인덱스 = 맵에 저장되는 preset 값. 순서를 바꾸면 기존 맵의 오브젝트가 바뀌므로 뒤에만 추가한다.
inline constexpr ObjectPreset kObjectPresets[] = {
    {"빨강 3x2", 3, 2, {210, 70, 70, 255}},
    {"노랑 2x3", 2, 3, {220, 200, 80, 255}},
};

inline constexpr int ObjectPresetCount() {
    return static_cast<int>(sizeof(kObjectPresets) / sizeof(kObjectPresets[0]));
}

// 범위를 벗어나면 0번으로 안전 폴백(손상된 맵 대비).
inline const ObjectPreset& ObjectPresetAt(int index) {
    if (index < 0 || index >= ObjectPresetCount()) index = 0;
    return kObjectPresets[index];
}

} // namespace gs::core
