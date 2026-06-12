#pragma once

#include "math/Rect.h"
#include "math/Vector2D.h"
#include "world/FootholdMap.h"
#include "world/Portal.h"
#include "world/TileMap.h"

#include <string>
#include <vector>

// 에디터가 편집하고 게임이 로드하는 단일 맵 문서: 타일 격자(시각) + 풋홀드(충돌)
// + 스폰 위치 + 포탈(맵 연결). 1차는 의존성 없는 경량 텍스트 포맷.
// P-006에서 JSON + DataManager로 승격(ADR-005).
namespace gs::world {

class Map {
public:
    TileMap&            Tiles()           { return m_tiles; }
    const TileMap&      Tiles()     const { return m_tiles; }
    FootholdMap&        Footholds()       { return m_footholds; }
    const FootholdMap&  Footholds() const { return m_footholds; }

    math::Vector2D Spawn() const           { return m_spawn; }
    void           SetSpawn(math::Vector2D v) { m_spawn = v; }

    std::vector<Portal>&       Portals()       { return m_portals; }
    const std::vector<Portal>& Portals() const { return m_portals; }

    // 월드 경계(픽셀): 타일 격자 W×H × 타일크기. 카메라/플레이어 클램프와 경계 렌더에 쓴다.
    math::Rect WorldBounds() const {
        const float s = static_cast<float>(m_tiles.TileSize());
        return {0.0f, 0.0f, m_tiles.Width() * s, m_tiles.Height() * s};
    }

    // 실패 시 std::runtime_error를 던진다(ADR-005 파싱 견고성).
    void Save(const std::string& path) const;
    void Load(const std::string& path);

private:
    TileMap             m_tiles;
    FootholdMap         m_footholds;
    math::Vector2D      m_spawn{200.0f, 560.0f}; // 기본 스폰(v1 맵 하위호환)
    std::vector<Portal> m_portals;
};

} // namespace gs::world
