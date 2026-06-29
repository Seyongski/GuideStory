#pragma once

#include "EditorScreen.h"

#include "core/Camera.h"
#include "core/Ui.h"
#include "editor/MapEditor.h"
#include "world/Map.h"

#include <optional>
#include <string>
#include <vector>

namespace gs::app {

// 맵 에디터 화면. 들어오면 빈 상태(문서 없음)로 시작한다.
//  - 빈 상태  : [새로 만들기]/[열기]/[메뉴로] 세로 메뉴. 화면엔 안내만.
//  - 편집 상태: editor::MapEditor에 위임 + 상단 GUI 툴바.
//    상단 = [둘러보기][격자][추가▼][배경][카메라▼] ... [파일▼]. 드롭다운으로 동작을 묶는다.
//      · 추가  : 타일/풋홀드/스폰/포탈/오브젝트(배치 모드)
//      · 카메라: 화면(파랑)/이동범위(빨강) 사각형 편집
//      · 파일  : 새로 만들기/열기/저장/다른 이름으로 저장/메뉴로
//    타일·오브젝트 모드는 우측에 검색 가능한 팔레트 오버레이가 뜬다(종류 선택).
// "열기/새로 만들기"로 문서를 만든 뒤에야 편집·저장이 가능하다.
class MapEditorScreen final : public EditorScreen {
public:
    MapEditorScreen();

    EditorScene Update(const platform::Input& in, float dt) override;
    void Render(platform::IRenderDevice& r) override;
    CloseDecision OnCloseRequest() override; // 창 닫기 시 저장 안 된 변경 확인

private:
    void StartEditing();              // 기본 맵 + MapEditor 생성(편집 상태로 전환)

    // 편집 화면을 떠나기 전(ESC·메뉴로) 저장 안 된 변경을 확인한다.
    // 변경 없으면 Launcher로, 있으면 확인창 결과대로(저장/버림→Launcher, 취소→Stay).
    EditorScene ConfirmLeave();

    // 현재 모드(타일/오브젝트)에 맞는 우측 팔레트 검색 칸. 둘 다 아니면 nullptr.
    ui::SearchBox* ActiveSearch();
    // 우측 팔레트 목록을 검색어로 필터해 m_paletteList/m_paletteIds에 다시 채운다.
    void BuildPalette(bool tiles, const std::string& query);

    world::Map                  m_map;
    core::Camera                m_camera;
    std::optional<editor::MapEditor> m_editor; // 편집 상태에서만 존재

    ui::Menu     m_emptyMenu;  // 빈 상태 메뉴
    ui::Toolbar  m_quick;      // 상단: [둘러보기][격자][배경] 단일 버튼들
    ui::Dropdown m_fileMenu;   // 상단(우): 파일(새로/열기/저장/다른이름/메뉴로)
    ui::Dropdown m_addMenu;    // 상단: 추가(타일/풋홀드/스폰/포탈/오브젝트)
    ui::Dropdown m_camMenu;    // 상단: 카메라(화면/이동범위)
    ui::Slider   m_zoomBar;    // 상단: 둘러보기 확인용 에디터 줌(저장 안 됨, 게임 영향 없음)

    // 우측 팔레트(타일/오브젝트 공용). 검색어로 필터해 매 프레임 다시 채운다.
    ui::SearchBox    m_tileSearch;  // 타일 종류 검색
    ui::SearchBox    m_objSearch;   // 오브젝트 종류 검색
    ui::Toolbar      m_paletteList; // 필터된 항목 버튼들(세로)
    std::vector<int> m_paletteIds;  // 목록 인덱스 → 실제 id(타일 번호) 또는 프리셋 인덱스

    // 사각형 편집 상태. 0=없음, 1=이동범위(빨강), 2=화면(파랑).
    //  - 좌클릭: 하얀 핸들(8개: 모서리+변 중앙)을 잡아 크기 조절(정밀).
    //  - 우드래그: 처음부터 새 사각형을 그린다(우클릭만=0크기→해제).
    int            m_editingRect = 0;
    int            m_activeHandle = -1; // 좌클릭으로 잡은 핸들(0~7), 없으면 -1
    bool           m_rectDragging = false; // 우드래그(새로 그리기) 진행 중
    math::Vector2D m_rectStart{};          // 우드래그 시작(월드 좌표)
};

} // namespace gs::app
