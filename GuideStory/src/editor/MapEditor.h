#pragma once

#include "core/Camera.h"
#include "math/Vector2D.h"
#include "platform/IRenderDevice.h"
#include "platform/Input.h"
#include "world/Map.h"

#include <string>

// 인엔진 맵 에디터. 마우스로 타일/풋홀드/스폰/포탈을 배치하고 텍스트 맵으로 저장한다.
// 포탈 대상 맵·맵 크기는 텍스트 입력(타이핑)으로 지정한다.
namespace gs::editor {

// 편집 모드 — Tab으로 순환. 모드별 좌클릭 동작이 다르다.
enum class EditMode { Tile, Foothold, Spawn, Portal };

class MapEditor {
public:
    explicit MapEditor(world::Map& map) : m_map(map) {}

    // 에디터 모드일 때 매 프레임. 방향키로 카메라 패닝, 마우스로 편집, 타이핑으로 입력.
    void Update(const platform::Input& in, core::Camera& cam, float dt);

    // 그리드/펜딩/스폰/포탈라벨/상태 텍스트 등 편집 오버레이.
    void Render(platform::IRenderDevice& r, const core::Camera& cam) const;

    world::TileId CurrentTile() const { return m_currentTile; }
    EditMode      Mode()        const { return m_mode; }

    // 텍스트 입력 필드가 열려 있는가(EditorApp가 ESC=취소/종료를 구분하는 데 사용).
    bool IsTextActive() const { return m_textActive; }

    const std::string& LastStatus() const { return m_status; }

private:
    // 텍스트 입력 대상.
    enum class TextTarget { None, Resize, PortalTarget, SaveAs, Open };

    math::Vector2D SnapToGrid(math::Vector2D world) const;
    int  PortalAt(math::Vector2D world) const;     // 히트된 포탈 인덱스, 없으면 -1
    void CycleMode();
    void BeginTextEntry(TextTarget target, std::string initial);
    void CommitText();
    void RefreshNextIds(); // 로드/새맵 후 다음 풋홀드·포탈 id를 최대값+1로 복원

    world::Map&    m_map;
    world::TileId  m_currentTile = 1;
    EditMode       m_mode = EditMode::Tile;
    bool           m_showGrid = true;

    bool           m_hasPending = false; // 풋홀드 첫 점 찍힘
    math::Vector2D m_pendingPoint{};
    int            m_nextFootholdId = 1;
    int            m_nextPortalId = 1;
    int            m_selectedPortal = -1; // m_map.Portals() 인덱스

    // 텍스트 입력 상태.
    bool           m_textActive = false;
    std::string    m_textBuffer;
    TextTarget     m_textTarget = TextTarget::None;

    std::string    m_mapPath = "field01.gsmap"; // 실행 폴더(x64/Debug) 기준
    std::string    m_status;
};

} // namespace gs::editor
