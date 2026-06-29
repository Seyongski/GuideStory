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
