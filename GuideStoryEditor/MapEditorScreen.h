#pragma once

#include "EditorScreen.h"

#include "core/Camera.h"
#include "core/Ui.h"
#include "editor/MapEditor.h"
#include "world/Map.h"

#include <optional>

namespace gs::app {

// 맵 에디터 화면. 들어오면 빈 상태(문서 없음)로 시작한다.
//  - 빈 상태  : [새로 만들기]/[열기]/[메뉴로] 세로 메뉴. 화면엔 안내만.
//  - 편집 상태: editor::MapEditor에 위임 + 상단 GUI 툴바(모드 버튼 + 파일 버튼).
//    기본은 둘러보기(드래그 패닝). 격자/타일/풋홀드/스폰/포탈 버튼으로 모드를 바꾼다.
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

    world::Map                  m_map;
    core::Camera                m_camera;
    std::optional<editor::MapEditor> m_editor; // 편집 상태에서만 존재

    ui::Menu    m_emptyMenu;  // 빈 상태 메뉴
    ui::Toolbar m_tools;      // 편집: 둘러보기/격자/타일/풋홀드/스폰/포탈/배경/오브젝트
    ui::Toolbar m_files;      // 편집: 새로/열기/저장/다른이름/메뉴로
    ui::Toolbar m_objPalette; // 우측 패널: 오브젝트 프리셋 목록(Object 모드일 때만 표시)
    ui::Slider  m_zoomBar;    // 상단: 둘러보기 확인용 에디터 줌(저장 안 됨, 게임 영향 없음)
    ui::Toolbar m_rectTools;  // 상단: 이동범위(빨강)/화면(파랑) 지정 토글 2버튼

    // 사각형 편집 상태. 0=없음, 1=이동범위(빨강), 2=화면(파랑).
    //  - 좌클릭: 하얀 핸들(8개: 모서리+변 중앙)을 잡아 크기 조절(정밀).
    //  - 우드래그: 처음부터 새 사각형을 그린다(우클릭만=0크기→해제).
    int            m_editingRect = 0;
    int            m_activeHandle = -1; // 좌클릭으로 잡은 핸들(0~7), 없으면 -1
    bool           m_rectDragging = false; // 우드래그(새로 그리기) 진행 중
    math::Vector2D m_rectStart{};          // 우드래그 시작(월드 좌표)
};

} // namespace gs::app
