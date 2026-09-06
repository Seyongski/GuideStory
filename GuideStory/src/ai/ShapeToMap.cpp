#include "ai/ShapeToMap.h"

#include <algorithm>
#include <cmath>

namespace gs::ai {

namespace {

constexpr int kNoSurface = -1;

// 열 x의 표면 높이(셀 y). 타일이 있고 **바로 윗 칸이 빈** 셀 중 가장 위. 없으면 kNoSurface.
// 격자 위쪽 밖은 빈 칸으로 친다 — 도형이 맨 윗줄에 닿아도 그 줄이 표면이다.
int SurfaceRow(const ShapeResult& shape, int x) {
    for (int y = 0; y < shape.h; ++y) {
        if (shape.At(x, y) == world::kEmptyTile) continue;
        const bool aboveEmpty = (y == 0) || (shape.At(x, y - 1) == world::kEmptyTile);
        if (aboveEmpty) return y;
    }
    return kNoSurface;
}

// 세 점이 한 직선 위인가(정수 좌표라 외적이 정확히 0인지 보면 된다).
bool Collinear(int ax, int ay, int bx, int by, int cx, int cy) {
    return (bx - ax) * (cy - ay) == (by - ay) * (cx - ax);
}

} // namespace

std::vector<world::Foothold> ExtractFootholds(const ShapeResult& shape,
                                              int cellX, int cellY, int tileSize,
                                              int nextId, int minRunCells) {
    std::vector<world::Foothold> out;
    if (!shape.Valid() || tileSize <= 0) return out;

    const float ts = static_cast<float>(tileSize);
    if (minRunCells < 1) minRunCells = 1;

    // 1) 열별 표면 높이
    std::vector<int> surface(static_cast<std::size_t>(shape.w), kNoSurface);
    for (int x = 0; x < shape.w; ++x) surface[static_cast<std::size_t>(x)] = SurfaceRow(shape, x);

    // 2) 끊기지 않는 구간(열이 이어지고 높이차 <= 1)마다 폴리라인을 만든다
    int runStart = -1;
    auto flush = [&](int startX, int endX) {   // [startX, endX] 닫힌 구간
        const int cells = endX - startX + 1;
        if (cells < minRunCells) return;       // 파편은 버린다

        // 표면 점은 **칸 중앙**에 찍는다. 왼쪽 모서리에 찍으면 계단이 경사가 아니라
        // 계단으로 남는다(마지막 칸에 평평한 꼬리가 생겨 선분이 둘로 쪼개진다).
        // 대신 양 끝은 국소 기울기대로 반 칸 연장해 선분이 구간 전체를 덮게 한다 —
        // 그래야 캐릭터가 발판 끝에서 헛디디지 않는다.
        //
        // 좌표를 2배로 잡아 정수로만 계산한다(반 칸이 0.5가 되어 부동소수 비교가 필요해지는 것을 피한다).
        const auto h = [&](int x) { return surface[static_cast<std::size_t>(x)]; };
        const int dFirst = (cells >= 2) ? (h(startX + 1) - h(startX)) : 0;
        const int dLast  = (cells >= 2) ? (h(endX) - h(endX - 1))     : 0;

        std::vector<std::pair<int, int>> pts;   // (2x, 2y)
        pts.reserve(static_cast<std::size_t>(cells) + 2);
        pts.emplace_back(2 * startX, 2 * h(startX) - dFirst);
        for (int x = startX; x <= endX; ++x) pts.emplace_back(2 * x + 1, 2 * h(x));
        pts.emplace_back(2 * (endX + 1), 2 * h(endX) + dLast);

        // 3) 일직선 위의 중간 점 제거 — 평지/경사는 선분 하나로 합쳐진다
        std::vector<std::pair<int, int>> simple;
        simple.reserve(pts.size());
        for (const auto& p : pts) {
            while (simple.size() >= 2 &&
                   Collinear(simple[simple.size() - 2].first, simple[simple.size() - 2].second,
                             simple.back().first, simple.back().second,
                             p.first, p.second)) {
                simple.pop_back();
            }
            simple.push_back(p);
        }

        // 2배 좌표를 월드로: (x2/2 + cellX) * tileSize.
        const auto world = [&](std::pair<int, int> p) {
            return math::Vector2D{cellX * ts + p.first * ts * 0.5f,
                                  cellY * ts + p.second * ts * 0.5f};
        };
        for (std::size_t i = 0; i + 1 < simple.size(); ++i) {
            world::Foothold fh;
            fh.id = nextId++;
            fh.p1 = world(simple[i]);
            fh.p2 = world(simple[i + 1]);
            out.push_back(fh);
        }
    };

    for (int x = 0; x < shape.w; ++x) {
        const int h = surface[static_cast<std::size_t>(x)];
        if (h == kNoSurface) {                 // 빈 열 → 구간 종료
            if (runStart >= 0) { flush(runStart, x - 1); runStart = -1; }
            continue;
        }
        if (runStart < 0) { runStart = x; continue; }
        // 절벽(높이차 2칸 이상)이면 여기서 끊는다 — 수직 낙차를 발판으로 잇지 않는다.
        if (std::abs(h - surface[static_cast<std::size_t>(x - 1)]) > 1) {
            flush(runStart, x - 1);
            runStart = x;
        }
    }
    if (runStart >= 0) flush(runStart, shape.w - 1);

    return out;
}

ShapeApplyReport ApplyShape(world::Map& map, const ShapeResult& shape,
                            int cellX, int cellY, const ShapeApplyOptions& opt) {
    ShapeApplyReport report;
    if (!shape.Valid()) return report;

    world::TileMap& tiles = map.Tiles();
    const int tileSize = tiles.TileSize();
    if (tileSize <= 0) return report;

    // 1) 타일 새기기. 맵 밖은 조용히 버린다 — 경계를 넘겨 붙여도 안전해야 한다.
    //    빈 칸(0)은 덮어쓰지 않는다. 도형 주변의 기존 편집을 지우면 "제안"이 아니라 파괴다.
    for (int y = 0; y < shape.h; ++y) {
        for (int x = 0; x < shape.w; ++x) {
            const world::TileId id = shape.At(x, y);
            if (id == world::kEmptyTile) continue;
            const int mx = cellX + x, my = cellY + y;
            if (!tiles.InBounds(mx, my)) continue;
            tiles.Set(mx, my, id);
            ++report.tilesWritten;
        }
    }

    if (!opt.addFootholds) return report;

    // 2) 풋홀드. 기존 id와 겹치지 않게 최대값+1부터 시작한다.
    int nextId = 1;
    for (const world::Foothold& fh : map.Footholds().All())
        nextId = std::max(nextId, fh.id + 1);

    const std::vector<world::Foothold> added =
        ExtractFootholds(shape, cellX, cellY, tileSize, nextId, opt.minRunCells);

    for (const world::Foothold& fh : added) {
        map.Footholds().Add(fh);
        ++report.footholdsAdded;
    }

    // 3) 스폰을 가장 긴 발판 가운데 위로. 도형만 만들어 놓고 설 자리가 없으면 맵이 아니다.
    if (opt.moveSpawn && !added.empty()) {
        const world::Foothold* best = nullptr;
        float bestLen = 0.0f;
        for (const world::Foothold& fh : added) {
            if (fh.IsWall()) continue;
            const float len = fh.MaxX() - fh.MinX();
            if (len > bestLen) { bestLen = len; best = &fh; }
        }
        if (best != nullptr) {
            const float midX = (best->p1.x + best->p2.x) * 0.5f;
            // 발판보다 살짝 위에 둔다 — 정확히 같은 y면 첫 프레임에 바닥 판정이 흔들린다.
            map.SetSpawn({midX, best->SurfaceY(midX) - 2.0f});
            report.spawnMoved = true;
        }
    }

    return report;
}

} // namespace gs::ai
