#pragma once

#include "core/Camera.h"
#include "math/Vector2D.h"
#include "platform/IRenderDevice.h"
#include "platform/Input.h"
#include "world/Map.h"

#include <string>

// 인엔진 맵 에디터. 기본은 둘러보기(드래그로 패닝) 모드이고, 호스트의 GUI 툴바가
// 모드(타일/풋홀드/스폰/포탈)와 격자 표시를 전환한다. 텍스트 맵으로 저장한다.
// 포탈 대상 맵·맵 크기는 텍스트 입력(타이핑)으로 지정한다.
namespace gs::editor {

// 편집 모드. Browse는 배치 없이 좌드래그로 화면을 이동(둘러보기)한다.
// 나머지는 좌클릭으로 각 요소를 배치한다. 전환은 GUI 툴바 버튼이 담당한다.
// Object는 우측 팔레트에서 고른 프리셋을 좌클릭으로 배치(우클릭 삭제), 고스트 미리보기.
enum class EditMode { Browse, Tile, Foothold, Spawn, Portal, Object };

class MapEditor {
public:
    explicit MapEditor(world::Map& map) : m_map(map) {}

    // 매 프레임. 방향키/좌드래그로 패닝, 마우스로 편집, 타이핑으로 입력.
    // allowMouse=false면 마우스 편집/드래그를 무시한다(포인터가 GUI 툴바 위일 때).
    void Update(const platform::Input& in, core::Camera& cam, float dt, bool allowMouse = true);

    // 그리드/펜딩/스폰/포탈라벨/상태 텍스트 등 편집 오버레이.
    void Render(platform::IRenderDevice& r, const core::Camera& cam) const;

    world::TileId CurrentTile() const { return m_currentTile; }
    EditMode      Mode()        const { return m_mode; }

    // 모드 전환 — 배치 모드(Browse 외)로 들어가면 격자를 자동으로 켠다.
    void SetMode(EditMode m);
    void ToggleGrid() { m_showGrid = !m_showGrid; }
    bool GridOn() const { return m_showGrid; }

    // 배경 이미지 설정/조회. 절대 경로를 받아도 파일명만 맵에 저장한다(assets/backgrounds 기준).
    void SetBackground(const std::string& pathOrName);
    const std::string& Background() const { return m_map.Background(); }

    // 맵 격자 크기를 배경 픽셀 크기를 덮도록 맞춘다(올림). 배경 크기는 호출측이 렌더에서 구해 전달.
    void FitToBackground(int wpx, int hpx);

    // 렌더에서 측정한 배경 원본 픽셀 크기를 맵에 반영한다(저장 시 BGSIZE로 기록 → 게임이 정확한 WorldBounds).
    // 배경이 맵의 카메라/시각 범위 권위이므로, 알게 되는 즉시 맵에 심어 둔다.
    void SetBackgroundSize(math::Vector2D px) { m_map.SetBackgroundSize(px); }

    // 오브젝트 프리셋 선택 → Object 모드로 전환(우측 팔레트가 호출). 현재 프리셋 조회.
    void SelectObject(int preset);
    int  CurrentObject() const { return m_currentObject; }

    // 파일 동작 — 키보드 단축키(N/S/Shift+S/L)와 GUI 툴바가 공유하는 진입점.
    void NewMap();       // 새 맵: 먼저 이름을 정한 뒤(저장 대화상자) 기본 캔버스를 만들고 저장
    void CreateDefault(const std::string& path); // 기본 캔버스 생성 + 경로 지정 + 즉시 저장(이름 결정 후 호출)
    void Save();         // 현재 파일로 저장(아직 저장한 적 없으면 SaveAs로 위임)
    void SaveAs();       // 다른 이름으로 저장(네이티브 파일 대화상자)
    void Open();         // 열기(네이티브 파일 대화상자)
    const std::string& MapPath() const { return m_mapPath; }

    // 텍스트 입력 필드가 열려 있는가(호스트가 ESC=취소/뒤로를 구분하는 데 사용).
    bool IsTextActive() const { return m_textActive; }

    const std::string& LastStatus() const { return m_status; }

private:
    // 텍스트 입력 대상(파일 열기/저장은 네이티브 대화상자로 분리됨).
    enum class TextTarget { None, Resize, PortalTarget };

    math::Vector2D SnapToGrid(math::Vector2D world) const;
    math::Vector2D SnapTopLeft(math::Vector2D world) const; // 좌상단을 셀에 내림 정렬(오브젝트)
    int  PortalAt(math::Vector2D world) const;     // 히트된 포탈 인덱스, 없으면 -1
    int  ObjectAt(math::Vector2D world) const;     // 히트된 오브젝트 인덱스(위 우선), 없으면 -1
    void BeginTextEntry(TextTarget target, std::string initial);
    void CommitText();
    void RefreshNextIds(); // 로드/새맵 후 다음 풋홀드·포탈 id를 최대값+1로 복원
    void RecomputeNextPortalId(); // 포탈 삭제 후 다음 id를 최대값+1로 정리(번호 누적 방지)

    world::Map&    m_map;
    world::TileId  m_currentTile = 1;
    int            m_currentObject = 0;  // 선택된 오브젝트 프리셋(core::ObjectPalette 인덱스)
    EditMode       m_mode = EditMode::Browse; // 기본: 둘러보기(드래그 패닝)
    bool           m_showGrid = false;

    bool           m_panning = false;    // 좌드래그 패닝 진행 중
    math::Vector2D m_panLast{};          // 직전 프레임 마우스 위치(드래그 델타용)

    math::Vector2D m_pointer{};          // 직전 마우스 화면 위치(오브젝트 고스트용)
    bool           m_pointerInCanvas = false; // 포인터가 캔버스(=GUI 밖)에 있는가

    bool           m_hasPending = false; // 풋홀드 첫 점 찍힘
    math::Vector2D m_pendingPoint{};
    int            m_nextFootholdId = 1;
    int            m_nextPortalId = 1;
    int            m_selectedPortal = -1; // m_map.Portals() 인덱스

    // 텍스트 입력 상태.
    bool           m_textActive = false;
    std::string    m_textBuffer;
    TextTarget     m_textTarget = TextTarget::None;

    std::string    m_mapPath;  // 저장/로드한 절대 경로. 비어 있으면 "아직 저장 안 됨".
    std::string    m_status;
};

} // namespace gs::editor
