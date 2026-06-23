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

// 배치된 오브젝트(건물 등). 1차는 단색 프리셋(core::ObjectPalette 인덱스) + 월드 좌상단 위치.
// 추후 preset이 스프라이트(아틀라스 srcRect)로 확장된다. 충돌은 풋홀드가 담당(ADR-008).
struct MapObject {
    int            preset = 0; // core::ObjectPalette 인덱스
    math::Vector2D pos{};      // 월드 좌상단(픽셀)
};

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

    std::vector<MapObject>&       Objects()       { return m_objects; }
    const std::vector<MapObject>& Objects() const { return m_objects; }

    // 배경 이미지 파일명(assets/backgrounds 기준, 공백 없는 파일명). 비어 있으면 배경 없음.
    const std::string& Background() const        { return m_background; }
    void               SetBackground(std::string name) {
        if (name != m_background) m_bgSize = {}; // 배경이 바뀌면 크기 미상으로 → 다시 측정 필요
        m_background = std::move(name);
    }

    // 배경 PNG의 원본 픽셀 크기. 0이면 미상(렌더 시 측정해 채운다). 배경이 맵의 시각·카메라 범위 권위다.
    math::Vector2D BackgroundSize() const           { return m_bgSize; }
    void           SetBackgroundSize(math::Vector2D s) { m_bgSize = s; }

    // 월드 경계(픽셀): 카메라/플레이어 클램프와 경계 렌더에 쓴다.
    //  - 배경이 있고 크기를 알면 그 배경 사각형이 권위다(타일 격자가 배경보다 커도 배경 밖
    //    빈 영역이 카메라에 노출되지 않게 — ADR-008 시각/충돌 이중관리의 정렬, 사용자 결정).
    //  - 배경이 없으면(또는 크기 미상이면) 타일 격자 W×H × 타일크기로 폴백한다.
    math::Rect WorldBounds() const {
        if (!m_background.empty() && m_bgSize.x > 0.0f && m_bgSize.y > 0.0f)
            return {0.0f, 0.0f, m_bgSize.x, m_bgSize.y};
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
    std::vector<Portal>    m_portals;
    std::vector<MapObject> m_objects;             // 배치된 오브젝트(건물 등)
    std::string            m_background;           // 배경 PNG 파일명(없으면 빈 문자열)
    math::Vector2D         m_bgSize{};             // 배경 원본 픽셀 크기(0 = 미상)
};

} // namespace gs::world
