#include "world/MapScaffold.h"

namespace gs::world {

void BuildDefaultMap(Map& map) {
    auto& tm = map.Tiles();
    tm.Resize(80, 40, 32); // 80x40 칸, 32px

    // 바닥 한 줄(시각용 흙 타일, 20행 = y 640).
    for (int x = 0; x < 50; ++x) tm.Set(x, 20, 1);
    for (int x = 22; x < 32; ++x) tm.Set(x, 15, 2); // 공중 발판(풀)

    // 충돌 풋홀드(원웨이 선분).
    auto& fhs = map.Footholds();
    fhs.Clear();
    fhs.Add({1, {0.0f, 640.0f},    {1600.0f, 640.0f}, 0, 0}); // 평지
    fhs.Add({2, {1600.0f, 640.0f}, {1900.0f, 520.0f}, 0, 0}); // 오르막 경사
    fhs.Add({3, {704.0f, 480.0f},  {1024.0f, 480.0f}, 0, 0}); // 공중 발판
}

} // namespace gs::world
