#include "core/WorldRenderer.h"

#include "core/ObjectPalette.h"
#include "platform/FileDialog.h" // BackgroundPath: 배경 파일명을 assets/backgrounds에 해석

namespace gs::core {

namespace {
// 타일 번호 → 색상(단색 1차 표현). 스프라이트 도입 시 이 매핑이 srcRect로 대체된다.
platform::Color TileColor(world::TileId id) {
    switch (id) {
        case 1: return {120, 90, 60, 255};   // 흙
        case 2: return {90, 160, 80, 255};    // 풀
        case 3: return {130, 130, 140, 255};  // 돌
        case 4: return {170, 140, 90, 255};   // 나무
        case 5: return {80, 120, 200, 255};   // 물/장식
        default: return {200, 80, 200, 255};  // 미정의 = 마젠타
    }
}
} // namespace

void RenderWorld(platform::IRenderDevice& r, const Camera& cam, const world::Map& map) {
    const float viewW = cam.ViewW();
    const float viewH = cam.ViewH();

    // 배경 PNG(맵에 설정 시) — 월드 원점에 원본 픽셀 1:1로(왜곡 없음), 모든 것 뒤에.
    if (!map.Background().empty()) {
        const platform::TextureId bg = r.LoadTexture(platform::BackgroundPath(map.Background()));
        if (bg != platform::kInvalidTexture) {
            const math::Vector2D sz = r.TextureSize(bg);
            r.DrawTexture(bg, cam.WorldRectToScreen({0.0f, 0.0f, sz.x, sz.y}));
        }
    }

    const auto& tm = map.Tiles();
    for (int y = 0; y < tm.Height(); ++y) {
        for (int x = 0; x < tm.Width(); ++x) {
            const world::TileId id = tm.At(x, y);
            if (id == world::kEmptyTile) continue;
            const math::Rect sr = cam.WorldRectToScreen(tm.CellRect(x, y));
            // 화면 밖 컬링.
            if (sr.x > viewW || sr.y > viewH || sr.x + sr.w < 0.0f || sr.y + sr.h < 0.0f)
                continue;
            r.FillRect(sr, TileColor(id));
        }
    }

    // 오브젝트(건물 등) — 단색 프리셋 사각형. 타일 위, 풋홀드/포탈 아래.
    const float ts = static_cast<float>(tm.TileSize());
    for (const auto& o : map.Objects()) {
        const ObjectPreset& pr = ObjectPresetAt(o.preset);
        const math::Rect sr = cam.WorldRectToScreen(
            {o.pos.x, o.pos.y, pr.wTiles * ts, pr.hTiles * ts});
        if (sr.x > viewW || sr.y > viewH || sr.x + sr.w < 0.0f || sr.y + sr.h < 0.0f)
            continue; // 화면 밖 컬링
        r.FillRect(sr, pr.color);
        r.DrawRect(sr, {0, 0, 0, 120}); // 외곽선
    }

    // 풋홀드(충돌선) — 디버그 초록선.
    for (const auto& fh : map.Footholds().All()) {
        const math::Vector2D a = cam.WorldToScreen(fh.p1);
        const math::Vector2D b = cam.WorldToScreen(fh.p2);
        r.DrawLine(a, b, {60, 220, 90, 255});
    }

    // 포탈 — 문 모양 사각형(pos = 바닥 중심). 연결 여부로 색 구분.
    for (const auto& p : map.Portals()) {
        const math::Rect sr = cam.WorldRectToScreen(PortalBox(p.pos));
        const platform::Color fill = p.targetMap.empty()
            ? platform::Color{150, 90, 180, 130}   // 미연결 = 흐린 보라
            : platform::Color{120, 200, 220, 150};  // 연결됨 = 청록
        r.FillRect(sr, fill);
        r.DrawRect(sr, {235, 235, 255, 255});
    }

    // 월드 경계 — 무한맵 느낌 해소용 외곽선.
    const math::Rect wb = cam.WorldRectToScreen(map.WorldBounds());
    r.DrawRect(wb, {255, 230, 120, 200});
}

} // namespace gs::core
