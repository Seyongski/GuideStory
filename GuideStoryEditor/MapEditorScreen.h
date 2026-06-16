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

private:
    void StartEditing();              // 기본 맵 + MapEditor 생성(편집 상태로 전환)

    world::Map                  m_map;
    core::Camera                m_camera;
    std::optional<editor::MapEditor> m_editor; // 편집 상태에서만 존재

    ui::Menu    m_emptyMenu;  // 빈 상태 메뉴
    ui::Toolbar m_tools;      // 편집: 둘러보기/격자/타일/풋홀드/스폰/포탈/배경/맞춤/오브젝트
    ui::Toolbar m_files;      // 편집: 새로/열기/저장/다른이름/메뉴로
    ui::Toolbar m_objPalette; // 우측 패널: 오브젝트 프리셋 목록(Object 모드일 때만 표시)

    // 배경 텍스처의 원본 픽셀 크기(렌더에서 갱신). "맞춤" 버튼이 이 값으로 맵 크기를 정한다.
    math::Vector2D m_bgSize{};
};

} // namespace gs::app
