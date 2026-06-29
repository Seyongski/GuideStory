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

// 맵에 배치된 몬스터 스프라이트. 1차는 정지 스프라이트(단일 이미지)만 — FSM/이동/애니메이션은 후속(ADR-007).
// sprite = assets/mob 기준 상대 경로(공백 없는 파일명). pos = 월드 바닥 중심(발 위치 → 풋홀드 위에 선다).
// size = 표시 픽셀 크기(w,h); 0이면 텍스처 원본 크기를 쓴다.
struct MapMob {
    std::string    sprite;
    math::Vector2D pos{};
    math::Vector2D size{};
};

class Map {
public:
    TileMap&            Tiles()           { return m_tiles; }
    const TileMap&      Tiles()     const { return m_tiles; }
    FootholdMap&        Footholds()       { return m_footholds; }
    const FootholdMap&  Footholds() const { return m_footholds; }

    math::Vector2D Spawn() const           { return m_spawn; }
    void           SetSpawn(math::Vector2D v) { m_spawn = v; }

    // 캐릭터 이동범위(빨강): 플레이어가 걸을 수 있는 영역 = 카메라가 보여줄 수 있는 한계(월드 좌표).
    // w/h=0이면 미설정 → 배경/타일 경계(WorldBounds) 사용.
    math::Rect PlayerBounds() const                 { return m_playerBounds; }
    void       SetPlayerBounds(const math::Rect& r) { m_playerBounds = r; }
    bool       HasPlayerBounds() const              { return m_playerBounds.w > 0.0f && m_playerBounds.h > 0.0f; }

    // 게임 화면(파랑): 이 영역이 게임 창에 꽉 차게 확대 출력된다 → 폭이 줌을 정한다(작을수록 줌인).
    // 카메라는 이 화면이 PlayerBounds 안에 머물도록 데드존으로 플레이어를 따라간다. w/h=0이면 미설정(줌 1).
    math::Rect CameraView() const                   { return m_cameraView; }
    void       SetCameraView(const math::Rect& r)   { m_cameraView = r; }
    bool       HasCameraView() const                { return m_cameraView.w > 0.0f && m_cameraView.h > 0.0f; }

    std::vector<Portal>&       Portals()       { return m_portals; }
    const std::vector<Portal>& Portals() const { return m_portals; }

    std::vector<MapObject>&       Objects()       { return m_objects; }
    const std::vector<MapObject>& Objects() const { return m_objects; }

    std::vector<MapMob>&       Mobs()       { return m_mobs; }
    const std::vector<MapMob>& Mobs() const { return m_mobs; }

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

    // 카메라·플레이어 제한 경계: 이동범위(빨강)가 있으면 그것, 없으면 배경/타일 경계.
    // 게임이 카메라 클램프·플레이어 좌우벽·낙사 판정에 이 값을 쓴다(에디터 패닝은 WorldBounds 사용).
    math::Rect CameraBounds() const {
        return HasPlayerBounds() ? m_playerBounds : WorldBounds();
    }

    // 실패 시 std::runtime_error를 던진다(ADR-005 파싱 견고성).
    void Save(const std::string& path) const;
    void Load(const std::string& path);

    // 맵을 텍스트 포맷 문자열로 직렬화한다(Save가 파일에 쓰는 것과 동일 내용).
    //  - includeBgSize=false면 BGSIZE 줄을 뺀다 → 에디터의 "변경됨" 비교용. 배경 픽셀 크기는
    //    렌더에서 자동 측정되는 값이라 사용자 편집이 아니므로 변경 감지에서 제외한다.
    std::string Serialize(bool includeBgSize = true) const;

private:
    TileMap             m_tiles;
    FootholdMap         m_footholds;
    math::Vector2D      m_spawn{200.0f, 560.0f}; // 기본 스폰(v1 맵 하위호환)
    math::Rect          m_playerBounds{};         // 캐릭터 이동범위(빨강). w/h=0=미설정
    math::Rect          m_cameraView{};           // 게임 화면(파랑, 줌 결정). w/h=0=미설정(줌 1)
    std::vector<Portal>    m_portals;
    std::vector<MapObject> m_objects;             // 배치된 오브젝트(건물 등)
    std::vector<MapMob>    m_mobs;                 // 배치된 몬스터 스프라이트
    std::string            m_background;           // 배경 PNG 파일명(없으면 빈 문자열)
    math::Vector2D         m_bgSize{};             // 배경 원본 픽셀 크기(0 = 미상)
};

} // namespace gs::world
