#pragma once

#include "math/Rect.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gs::world {

// 타일 번호. 0 = 빈 칸. 맵은 픽셀이 아니라 이 인덱스 배열로 저장된다(메모리 핵심).
using TileId = std::uint16_t;
inline constexpr TileId kEmptyTile = 0;

// 시각용 타일 격자(코스메틱). 충돌은 FootholdMap이 담당한다.
class TileMap {
public:
    TileMap() = default;
    TileMap(int width, int height, int tileSize) { Resize(width, height, tileSize); }

    void Resize(int width, int height, int tileSize) {
        m_width = width;
        m_height = height;
        m_tileSize = tileSize;
        m_tiles.assign(static_cast<std::size_t>(width) * height, kEmptyTile);
    }

    int Width()    const { return m_width; }
    int Height()   const { return m_height; }
    int TileSize() const { return m_tileSize; }

    bool InBounds(int cx, int cy) const {
        return cx >= 0 && cy >= 0 && cx < m_width && cy < m_height;
    }

    TileId At(int cx, int cy) const {
        return InBounds(cx, cy) ? m_tiles[Index(cx, cy)] : kEmptyTile;
    }
    void Set(int cx, int cy, TileId id) {
        if (InBounds(cx, cy)) m_tiles[Index(cx, cy)] = id;
    }

    // 셀(cx,cy)의 월드 사각형.
    math::Rect CellRect(int cx, int cy) const {
        const float s = static_cast<float>(m_tileSize);
        return {cx * s, cy * s, s, s};
    }

    // 월드 좌표 → 셀 인덱스.
    int CellX(float worldX) const { return static_cast<int>(std::floor(worldX / m_tileSize)); }
    int CellY(float worldY) const { return static_cast<int>(std::floor(worldY / m_tileSize)); }

    // 직렬화/일괄 접근용 원시 버퍼.
    const std::vector<TileId>& Raw() const { return m_tiles; }
    std::vector<TileId>&       Raw()       { return m_tiles; }

private:
    std::size_t Index(int cx, int cy) const {
        return static_cast<std::size_t>(cy) * m_width + cx;
    }

    int m_width = 0;
    int m_height = 0;
    int m_tileSize = 32;
    std::vector<TileId> m_tiles;
};

} // namespace gs::world
