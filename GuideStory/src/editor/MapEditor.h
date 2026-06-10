#pragma once

#include "core/Camera.h"
#include "math/Vector2D.h"
#include "platform/IRenderDevice.h"
#include "platform/Input.h"
#include "world/Map.h"

#include <string>

// 인엔진 맵 에디터. 마우스로 타일을 칠하고 풋홀드(충돌선)를 배치하며 텍스트 맵으로 저장한다.
namespace gs::editor {

class MapEditor {
public:
    explicit MapEditor(world::Map& map) : m_map(map) {}

    // 에디터 모드일 때 매 프레임. 방향키로 카메라 패닝, 마우스로 편집.
    void Update(const platform::Input& in, core::Camera& cam, float dt);

    // 그리드/펜딩 포인트 등 편집 오버레이. (타일·풋홀드 본체는 Game이 공통 렌더)
    void Render(platform::IRenderDevice& r, const core::Camera& cam) const;

    world::TileId CurrentTile() const { return m_currentTile; }
    bool FootholdMode() const { return m_footholdMode; }

    const std::string& LastStatus() const { return m_status; }

private:
    math::Vector2D SnapToGrid(math::Vector2D world) const;

    world::Map&    m_map;
    world::TileId  m_currentTile = 1;
    bool           m_footholdMode = false;
    bool           m_showGrid = true;

    bool           m_hasPending = false; // 풋홀드 첫 점 찍힘
    math::Vector2D m_pendingPoint{};
    int            m_nextFootholdId = 1;

    std::string    m_mapPath = "field01.gsmap"; // 실행 폴더(x64/Debug) 기준
    std::string    m_status;
};

} // namespace gs::editor
