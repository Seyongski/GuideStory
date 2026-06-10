#include "world/Map.h"

#include <fstream>
#include <stdexcept>
#include <string>

// 경량 텍스트 포맷 (GSMAP v1):
//   GSMAP 1
//   TILESIZE 32
//   SIZE <w> <h>
//   TILES
//   <w*h개의 타일 번호, 공백/줄바꿈 구분>
//   FOOTHOLDS <count>
//   <id x1 y1 x2 y2 prev next>   (count줄)
//   END
namespace gs::world {

void Map::Save(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) throw std::runtime_error("맵 저장 실패(파일 열기): " + path);

    out << "GSMAP 1\n";
    out << "TILESIZE " << m_tiles.TileSize() << "\n";
    out << "SIZE " << m_tiles.Width() << " " << m_tiles.Height() << "\n";

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

    int tileSize = 0, w = 0, h = 0;
    if (!(in >> tag >> tileSize) || tag != "TILESIZE" || tileSize <= 0)
        throw std::runtime_error("맵 형식 오류: TILESIZE");
    if (!(in >> tag >> w >> h) || tag != "SIZE" || w <= 0 || h <= 0)
        throw std::runtime_error("맵 형식 오류: SIZE");

    if (!(in >> tag) || tag != "TILES")
        throw std::runtime_error("맵 형식 오류: TILES 구분자");

    // 로컬에 먼저 채우고, 전부 성공한 뒤에만 멤버를 교체한다(강한 예외 안전성).
    TileMap tiles(w, h, tileSize);
    auto& raw = tiles.Raw();
    for (std::size_t i = 0; i < raw.size(); ++i) {
        unsigned int v = 0;
        if (!(in >> v)) throw std::runtime_error("맵 형식 오류: 타일 데이터 부족");
        raw[i] = static_cast<TileId>(v);
    }

    int count = 0;
    if (!(in >> tag >> count) || tag != "FOOTHOLDS" || count < 0)
        throw std::runtime_error("맵 형식 오류: FOOTHOLDS");

    FootholdMap fhmap;
    for (int i = 0; i < count; ++i) {
        Foothold f;
        if (!(in >> f.id >> f.p1.x >> f.p1.y >> f.p2.x >> f.p2.y >> f.prev >> f.next))
            throw std::runtime_error("맵 형식 오류: 풋홀드 데이터 부족");
        fhmap.Add(f);
    }

    m_tiles = std::move(tiles);
    m_footholds = std::move(fhmap);
}

} // namespace gs::world
