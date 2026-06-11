#include "EditorApp.h"

#include "core/WorldRenderer.h"
#include "world/MapScaffold.h"

#include <chrono>

namespace gs::app {

namespace {
// 1차: 윈도우 크기 고정 (IWindow에 크기 질의 추가 전까지 상수).
constexpr float kViewW = 1280.0f;
constexpr float kViewH = 720.0f;
} // namespace

EditorApp::EditorApp(platform::IWindow& window, platform::IRenderDevice& renderer)
    : m_window(window),
      m_renderer(renderer),
      m_camera(kViewW, kViewH),
      m_editor(m_map) {
    // 편집 기준이 되는 기본 맵으로 시작한다(빈 캔버스 대신). L 키로 저장본을 불러올 수 있다.
    world::BuildDefaultMap(m_map);
    m_camera.Follow({640.0f, 360.0f});
}

void EditorApp::Run() {
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

void EditorApp::Update(float dt) {
    const platform::Input& in = m_window.GetInput();
    // 항상 편집 모드. 카메라 패닝·타일 페인트·풋홀드 배치·저장/로드는 MapEditor가 처리한다.
    m_editor.Update(in, m_camera, dt);
}

void EditorApp::Render() {
    m_renderer.Clear({100, 149, 237, 255}); // 하늘색
    core::RenderWorld(m_renderer, m_camera, m_map);
    m_editor.Render(m_renderer, m_camera); // 그리드/펜딩/모드 배지 오버레이
    m_renderer.Present();
}

} // namespace gs::app
