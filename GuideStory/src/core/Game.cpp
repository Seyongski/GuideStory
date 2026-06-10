#include "core/Game.h"

#include <chrono>

namespace gs::core {

namespace {
// 1차: 윈도우 크기 고정 (IWindow에 크기 질의 추가 전까지 상수).
constexpr float kViewW = 1280.0f;
constexpr float kViewH = 720.0f;

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

Game::Game(platform::IWindow& window, platform::IRenderDevice& renderer)
    : m_window(window),
      m_renderer(renderer),
      m_camera(kViewW, kViewH),
      m_editor(m_map) {
    BuildDefaultMap();
    m_player.SetPosition({200.0f, 560.0f}); // 바닥(y=640) 위 — 떨어지며 착지
    m_camera.Follow(m_player.Position());
}

void Game::BuildDefaultMap() {
    auto& tm = m_map.Tiles();
    tm.Resize(80, 40, 32); // 80x40 칸, 32px

    // 바닥 한 줄(시각용 흙 타일, 20행 = y 640).
    for (int x = 0; x < 50; ++x) tm.Set(x, 20, 1);
    for (int x = 22; x < 32; ++x) tm.Set(x, 15, 2); // 공중 발판(풀)

    // 충돌 풋홀드(원웨이 선분).
    auto& fhs = m_map.Footholds();
    fhs.Clear();
    fhs.Add({1, {0.0f, 640.0f},    {1600.0f, 640.0f}, 0, 0}); // 평지
    fhs.Add({2, {1600.0f, 640.0f}, {1900.0f, 520.0f}, 0, 0}); // 오르막 경사
    fhs.Add({3, {704.0f, 480.0f},  {1024.0f, 480.0f}, 0, 0}); // 공중 발판
}

void Game::Run() {
    using clock = std::chrono::steady_clock;
    auto prev = clock::now();

    while (!m_window.ShouldClose()) {
        m_window.PollEvents();

        const auto now = clock::now();
        float dt = std::chrono::duration<float>(now - prev).count();
        prev = now;
        if (dt > 0.05f) dt = 0.05f; // 스파이크 클램프

        Update(dt);
        Render();
    }
}

void Game::Update(float dt) {
    const platform::Input& in = m_window.GetInput();

    if (in.WasPressed(platform::Key::Tab)) m_editMode = !m_editMode;

    if (m_editMode) {
        m_editor.Update(in, m_camera, dt);
        return;
    }

    // 플레이 모드: 입력 → 이동 의도 번역.
    physics::MoveIntent intent;
    if (in.IsDown(platform::Key::Left))  intent.moveX -= 1.0f;
    if (in.IsDown(platform::Key::Right)) intent.moveX += 1.0f;

    const bool jumpEdge = in.WasPressed(platform::Key::Space);
    if (jumpEdge && in.IsDown(platform::Key::Down)) {
        intent.dropDown = true; // ↓ + 점프 = 드롭다운
    } else if (jumpEdge) {
        intent.jump = true;
    }

    m_player.Update(intent, m_map.Footholds(), dt);
    m_camera.Follow(m_player.Position());
}

void Game::Render() {
    m_renderer.Clear({100, 149, 237, 255}); // 하늘색
    RenderWorld();
    RenderPlayer();
    if (m_editMode) m_editor.Render(m_renderer, m_camera);
    m_renderer.Present();
}

void Game::RenderWorld() {
    const auto& tm = m_map.Tiles();
    for (int y = 0; y < tm.Height(); ++y) {
        for (int x = 0; x < tm.Width(); ++x) {
            const world::TileId id = tm.At(x, y);
            if (id == world::kEmptyTile) continue;
            const math::Rect sr = m_camera.WorldRectToScreen(tm.CellRect(x, y));
            // 화면 밖 컬링.
            if (sr.x > kViewW || sr.y > kViewH || sr.x + sr.w < 0.0f || sr.y + sr.h < 0.0f)
                continue;
            m_renderer.FillRect(sr, TileColor(id));
        }
    }

    // 풋홀드(충돌선) — 디버그 초록선.
    for (const auto& fh : m_map.Footholds().All()) {
        const math::Vector2D a = m_camera.WorldToScreen(fh.p1);
        const math::Vector2D b = m_camera.WorldToScreen(fh.p2);
        m_renderer.DrawLine(a, b, {60, 220, 90, 255});
    }
}

void Game::RenderPlayer() {
    const math::Vector2D feet = m_player.Position();
    const math::Rect box{feet.x - m_playerW * 0.5f, feet.y - m_playerH, m_playerW, m_playerH};
    const math::Rect sr = m_camera.WorldRectToScreen(box);
    const platform::Color c = m_player.Grounded()
        ? platform::Color{230, 80, 80, 255}    // 지상 = 빨강
        : platform::Color{230, 160, 60, 255};   // 공중 = 주황
    m_renderer.FillRect(sr, c);
}

} // namespace gs::core
