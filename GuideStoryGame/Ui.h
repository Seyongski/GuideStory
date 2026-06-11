#pragma once

#include "math/Rect.h"
#include "platform/Color.h"
#include "platform/IRenderDevice.h"
#include "platform/Input.h"

#include <string>
#include <vector>

namespace gs::app::ui {

// 화면 중앙 정렬 텍스트. (cx, cy)는 글자 박스의 중심.
void DrawCenteredText(platform::IRenderDevice& r, const std::string& text,
                      float cx, float cy, float pixelHeight, const platform::Color& color);

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
