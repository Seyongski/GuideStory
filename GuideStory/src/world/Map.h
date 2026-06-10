#pragma once

#include "world/FootholdMap.h"
#include "world/TileMap.h"

#include <string>

// 에디터가 편집하고 게임이 로드하는 단일 맵 문서: 타일 격자(시각) + 풋홀드(충돌).
// 1차는 의존성 없는 경량 텍스트 포맷. P-006에서 JSON + DataManager로 승격(ADR-005).
namespace gs::world {

class Map {
public:
    TileMap&            Tiles()           { return m_tiles; }
    const TileMap&      Tiles()     const { return m_tiles; }
    FootholdMap&        Footholds()       { return m_footholds; }
    const FootholdMap&  Footholds() const { return m_footholds; }

    // 실패 시 std::runtime_error를 던진다(ADR-005 파싱 견고성).
    void Save(const std::string& path) const;
    void Load(const std::string& path);

private:
    TileMap     m_tiles;
    FootholdMap m_footholds;
};

} // namespace gs::world
