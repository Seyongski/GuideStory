#pragma once

#include "platform/Color.h"
#include "world/TileMap.h" // TileId

// 타일 종류 프리셋 — 단색 1차 표현(스프라이트/PNG 도입 전). ObjectPalette와 같은 패턴:
// 에디터 팔레트(우측 오버레이·검색)와 WorldRenderer(실제 렌더)가 이 한 표를 공유한다.
// 추후 color가 PNG 텍스처(아틀라스 srcRect)로 확장된다 — 그때 이 구조에 경로를 더한다.
namespace gs::core {

struct TilePreset {
    world::TileId   id;    // 맵에 저장되는 타일 번호(0=빈 칸은 프리셋에 없음)
    const char*     name;  // 팔레트 라벨(검색 대상)
    platform::Color color; // 단색 채움
};

// 순서는 표시 순서일 뿐 — id가 맵 저장 값이므로 색/이름만 바꿔도 기존 맵은 안전하다.
inline constexpr TilePreset kTilePresets[] = {
    {1, "흙",   {120,  90,  60, 255}},
    {2, "풀",   { 90, 160,  80, 255}},
    {3, "돌",   {130, 130, 140, 255}},
    {4, "나무", {170, 140,  90, 255}},
    {5, "물",   { 80, 120, 200, 255}},
};

inline constexpr int TilePresetCount() {
    return static_cast<int>(sizeof(kTilePresets) / sizeof(kTilePresets[0]));
}

inline const TilePreset& TilePresetAt(int index) {
    if (index < 0 || index >= TilePresetCount()) index = 0;
    return kTilePresets[index];
}

// 타일 번호 → 색상. 미정의 번호는 마젠타로 눈에 띄게(손상/구버전 맵 대비).
inline platform::Color TileColor(world::TileId id) {
    for (const TilePreset& t : kTilePresets)
        if (t.id == id) return t.color;
    return {200, 80, 200, 255};
}

} // namespace gs::core
