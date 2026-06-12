#include "world/Map.h"

#include <fstream>
#include <stdexcept>
#include <string>

// 경량 텍스트 포맷 (GSMAP v2):
//   GSMAP 2
//   TILESIZE 32
//   SIZE <w> <h>
//   SPAWN <x> <y>
//   TILES
//   <w*h개의 타일 번호, 공백/줄바꿈 구분>
//   FOOTHOLDS <count>
//   <id x1 y1 x2 y2 prev next>            (count줄)
//   PORTALS <count>
//   <id x y targetPortal targetMap>       (count줄; targetMap 빈 값은 "-")
//   END
//
// 로더는 태그 구동(tag-driven)이라 SPAWN/PORTALS가 없는 v1 파일도 그대로 읽는다
// (스폰 기본값, 포탈 없음). 필수 누락(TILESIZE/SIZE/TILES) 시 예외(ADR-005).
namespace gs::world {

namespace {
constexpr const char* kEmptyTarget = "-"; // 공백 없는 빈 대상 토큰
}

void Map::Save(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) throw std::runtime_error("맵 저장 실패(파일 열기): " + path);

    out << "GSMAP 2\n";
    out << "TILESIZE " << m_tiles.TileSize() << "\n";
    out << "SIZE " << m_tiles.Width() << " " << m_tiles.Height() << "\n";
    out << "SPAWN " << m_spawn.x << " " << m_spawn.y << "\n";

    out << "TILES\n";
    const auto& raw = m_tiles.Raw();
    const int w = m_tiles.Width();
    for (std::size_t i = 0; i < raw.size(); ++i) {
        out << raw[i] << (((i + 1) % w == 0) ? '\n' : ' ');
    }

    const auto& fhs = m_footholds.All();
    out << "FOOTHOLDS " << fhs.size() << "\n";
    for (const auto& f : fhs) {
        out << f.id << " " << f.p1.x << " " << f.p1.y << " "
            << f.p2.x << " " << f.p2.y << " " << f.prev << " " << f.next << "\n";
    }

    out << "PORTALS " << m_portals.size() << "\n";
    for (const auto& p : m_portals) {
        const std::string& tgt = p.targetMap.empty() ? std::string(kEmptyTarget) : p.targetMap;
        out << p.id << " " << p.pos.x << " " << p.pos.y << " "
            << p.targetPortal << " " << tgt << "\n";
    }

    out << "END\n";

    if (!out) throw std::runtime_error("맵 저장 실패(쓰기 중 오류): " + path);
}

void Map::Load(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("맵 로드 실패(파일 열기): " + path);

    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "GSMAP")
        throw std::runtime_error("맵 형식 오류: 헤더(GSMAP)");

    // 로컬에 먼저 채우고, 전부 성공한 뒤에만 멤버를 교체한다(강한 예외 안전성).
    int tileSize = 0, w = 0, h = 0;
    bool haveSize = false, haveTiles = false;
    TileMap tiles;
    FootholdMap fhmap;
    math::Vector2D spawn{200.0f, 560.0f};
    std::vector<Portal> portals;

    // 태그 구동 파싱: 다음 토큰을 보고 섹션을 분기한다. END 또는 EOF에서 종료.
    while (in >> tag) {
        if (tag == "END") break;

        if (tag == "TILESIZE") {
            if (!(in >> tileSize) || tileSize <= 0)
                throw std::runtime_error("맵 형식 오류: TILESIZE");
        } else if (tag == "SIZE") {
            if (!(in >> w >> h) || w <= 0 || h <= 0)
                throw std::runtime_error("맵 형식 오류: SIZE");
            haveSize = true;
        } else if (tag == "SPAWN") {
            if (!(in >> spawn.x >> spawn.y))
                throw std::runtime_error("맵 형식 오류: SPAWN");
        } else if (tag == "TILES") {
            if (!haveSize || tileSize <= 0)
                throw std::runtime_error("맵 형식 오류: TILES 앞에 TILESIZE/SIZE 필요");
            tiles = TileMap(w, h, tileSize);
            auto& raw = tiles.Raw();
            for (std::size_t i = 0; i < raw.size(); ++i) {
                unsigned int v = 0;
                if (!(in >> v)) throw std::runtime_error("맵 형식 오류: 타일 데이터 부족");
                raw[i] = static_cast<TileId>(v);
            }
            haveTiles = true;
        } else if (tag == "FOOTHOLDS") {
            int count = 0;
            if (!(in >> count) || count < 0)
                throw std::runtime_error("맵 형식 오류: FOOTHOLDS");
            for (int i = 0; i < count; ++i) {
                Foothold f;
                if (!(in >> f.id >> f.p1.x >> f.p1.y >> f.p2.x >> f.p2.y >> f.prev >> f.next))
                    throw std::runtime_error("맵 형식 오류: 풋홀드 데이터 부족");
                fhmap.Add(f);
            }
        } else if (tag == "PORTALS") {
            int count = 0;
            if (!(in >> count) || count < 0)
                throw std::runtime_error("맵 형식 오류: PORTALS");
            for (int i = 0; i < count; ++i) {
                Portal p;
                std::string tgt;
                if (!(in >> p.id >> p.pos.x >> p.pos.y >> p.targetPortal >> tgt))
                    throw std::runtime_error("맵 형식 오류: 포탈 데이터 부족");
                p.targetMap = (tgt == kEmptyTarget) ? std::string() : tgt;
                portals.push_back(p);
            }
        } else {
            throw std::runtime_error("맵 형식 오류: 알 수 없는 섹션 '" + tag + "'");
        }
    }

    if (!haveSize || !haveTiles)
        throw std::runtime_error("맵 형식 오류: SIZE/TILES 누락");

    m_tiles = std::move(tiles);
    m_footholds = std::move(fhmap);
    m_spawn = spawn;
    m_portals = std::move(portals);
}

} // namespace gs::world
