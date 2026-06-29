#pragma once

#include "math/Rect.h"
#include "platform/Color.h"
#include "platform/IRenderDevice.h"
#include "platform/Input.h"

#include <string>
#include <vector>

// 게임/에디터 두 앱이 공유하는 경량 UI 위젯(ADR-009: 공통 코드 라이브러리 추출).
// SDL 비의존 — IRenderDevice/Input 인터페이스에만 의존한다(ADR-006).
namespace gs::app::ui {

// 화면 중앙 정렬 텍스트. (cx, cy)는 글자 박스의 중심.
void DrawCenteredText(platform::IRenderDevice& r, const std::string& text,
                      float cx, float cy, float pixelHeight, const platform::Color& color);

// 가로로 늘어선 마우스 전용 툴바. Menu와 달리 방향키/Enter를 소비하지 않으므로
// 카메라 패닝(방향키)·편집 단축키와 공존하는 편집 화면 상단 툴바에 쓴다.
// Menu와 동일한 Update/Render 분리 구조.
class Toolbar {
public:
    void Add(std::string label) { m_items.push_back({std::move(label)}); }
    void Clear() { m_items.clear(); m_hover = -1; m_active = -1; } // 동적으로 다시 채울 때(필터된 팔레트)

    // startX부터 오른쪽으로 (btnW + gap) 간격, 공통 y/높이로 배치.
    void LayoutRow(float startX, float y, float btnW, float btnH, float gap);

    // startY부터 아래로 (btnH + gap) 간격, 공통 x/너비로 배치(세로 팔레트).
    void LayoutColumn(float x, float startY, float btnW, float btnH, float gap);

    // 입력 처리. 클릭된 버튼 인덱스를 반환, 없으면 -1. (호버 강조는 다음 Render에 반영)
    int Update(const platform::Input& in);

    // 현재 활성(선택)된 버튼 인덱스. 호버처럼 강조해 토글/모드 상태를 보여준다. -1이면 없음.
    void SetActive(int index) { m_active = index; }

    // 버튼 활성/비활성. 비활성 버튼은 호버·클릭에 반응하지 않고 라벨이 회색으로 흐려진다.
    void SetEnabled(int index, bool enabled);

    void Render(platform::IRenderDevice& r) const;

private:
    struct Item {
        std::string label;
        math::Rect  rect{};
        bool        enabled = true;
    };

    std::vector<Item> m_items;
    int               m_hover = -1;  // 호버 강조 대상(Update가 갱신).
    int               m_active = -1; // 활성 강조 대상(호출측이 SetActive로 지정).
};

// 가로 슬라이더(프로그래스바형). 트랙을 드래그하거나 클릭해 값을 [min,max]에서 고른다.
// 마우스 전용 — 방향키/Enter를 소비하지 않아 편집 화면 단축키와 공존한다(Toolbar와 동일 정책).
class Slider {
public:
    // 값 범위와 초기값. (예: 50~200, 100 = 줌 퍼센트)
    void Setup(float minVal, float maxVal, float value);

    // 트랙 사각형(화면 픽셀). 핸들은 이 안에서 좌우로 움직인다.
    void Layout(float x, float y, float w, float h);

    // 입력 처리. 이번 프레임에 값이 바뀌면 true(드래그/클릭). 내부 값은 항상 최신으로 유지.
    bool Update(const platform::Input& in);

    void Render(platform::IRenderDevice& r) const;

    float Value() const { return m_value; }
    void  SetValue(float v);
    bool  Dragging() const { return m_dragging; } // 드래그 중이면 캔버스 입력을 막는 데 쓴다.

private:
    math::Rect m_rect{};
    float m_min = 0.0f, m_max = 1.0f, m_value = 0.0f;
    bool  m_hover = false;
    bool  m_dragging = false;
};

// 드롭다운 메뉴: 헤더 버튼(상단 스트립) + 클릭하면 그 아래로 펼쳐지는 세로 목록.
// 파일/추가/카메라처럼 버튼 하나에 여러 동작을 묶을 때 쓴다. 마우스 전용(편집 단축키와 공존).
// 펼쳐진 목록은 캔버스 위에 겹쳐 그려야 하므로 RenderHeader(스트립)와 RenderPopup(맨 위)을 분리한다.
// 여러 드롭다운의 "한 번에 하나만 열림"은 호출측이 toggled 신호를 보고 나머지를 Close()해서 맞춘다.
class Dropdown {
public:
    void SetLabel(std::string label) { m_label = std::move(label); }
    void Add(std::string item)       { m_items.push_back({std::move(item), {}}); }

    // 헤더 버튼 위치. 목록 항목은 이 버튼 바로 아래로 자동 배치된다.
    void LayoutButton(float x, float y, float w, float h);
    // 펼친 목록 항목의 크기/간격(기본값 있음). LayoutButton 뒤에 부르면 다시 배치한다.
    void SetItemSize(float w, float h, float gap);

    bool IsOpen() const { return m_open; }
    void Open()  { m_open = true; }
    void Close() { m_open = false; }
    void SetActive(bool a) { m_active = a; } // 헤더 강조(현재 그 그룹의 모드일 때)

    // 한 프레임 입력. item>=0 = 그 항목이 선택됨(목록 닫힘). toggled = 헤더를 눌러 열고/닫음.
    struct Result { int item = -1; bool toggled = false; };
    Result Update(const platform::Input& in);

    // 포인터가 헤더(또는 열려 있을 때 목록) 위인가 — 캔버스 입력 차단 판단용.
    bool PointerOver(math::Vector2D p) const;

    void RenderHeader(platform::IRenderDevice& r) const; // 상단 스트립에 그린다
    void RenderPopup(platform::IRenderDevice& r) const;  // 열려 있으면 맨 위에 덧그린다

private:
    void RelayoutItems();

    struct Item { std::string label; math::Rect rect{}; };
    std::string       m_label;
    std::vector<Item> m_items;
    math::Rect m_button{};
    float m_itemW = 150.0f, m_itemH = 30.0f, m_gap = 2.0f;
    bool  m_open = false;
    bool  m_active = false;
    int   m_hover = -1;
};

// 한 줄 검색 입력 칸. 클릭하면 포커스되어 타이핑을 받는다(한글=UTF-8 다중바이트 백스페이스 처리).
// 팔레트(타일/오브젝트) 상단에 두어 목록을 이름으로 거른다. 포커스 중에는 호출측이 편집 단축키를 막는다.
class SearchBox {
public:
    void Layout(float x, float y, float w, float h) { m_rect = {x, y, w, h}; }
    void SetPlaceholder(std::string p) { m_placeholder = std::move(p); }

    // 한 프레임 입력(포커스 토글 + 타이핑). 텍스트가 바뀌면 true(목록 다시 필터).
    bool Update(const platform::Input& in);
    void Render(platform::IRenderDevice& r) const;

    const std::string& Text() const { return m_text; }
    bool Focused() const     { return m_focused; }
    void SetFocused(bool f)  { m_focused = f; }
    void Clear()             { m_text.clear(); }

private:
    math::Rect  m_rect{};
    std::string m_text;
    std::string m_placeholder;
    bool        m_focused = false;
    bool        m_hover = false;
};

// 세로로 쌓이는 버튼 메뉴. 마우스 호버/클릭과 위/아래/Enter 키를 함께 지원한다.
// 활성화 로직만 담당하고, "무엇을 할지"는 호출측이 반환된 인덱스로 결정한다.
class Menu {
public:
    void Add(std::string label) { m_items.push_back({std::move(label)}); }

    // 첫 버튼의 중심 y부터 아래로 (btnH + gap) 간격으로 배치. centerX는 모든 버튼 공통.
    void Layout(float centerX, float firstCenterY, float btnW, float btnH, float gap);

    // 입력 처리. 활성화된(클릭 또는 Enter) 항목 인덱스를 반환, 없으면 -1.
    int Update(const platform::Input& in);

    void Render(platform::IRenderDevice& r) const;

private:
    struct Item {
        std::string label;
        math::Rect  rect{};
    };

    std::vector<Item> m_items;
    int               m_selected = 0; // 키보드/호버 강조 대상.
};

} // namespace gs::app::ui
