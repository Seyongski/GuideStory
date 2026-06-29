#include "core/Ui.h"

#include <algorithm>

namespace gs::app::ui {

namespace {
// 버튼 색 팔레트.
constexpr platform::Color kBtnFill   {45, 52, 74, 235};
constexpr platform::Color kBtnFillSel {70, 110, 190, 245}; // 호버/선택 강조
constexpr platform::Color kBtnBorder {120, 140, 180, 255};
constexpr platform::Color kBtnText   {235, 240, 250, 255};
constexpr platform::Color kBtnTextOff{120, 128, 145, 255}; // 비활성 라벨(회색)
constexpr platform::Color kTrackFill {30, 36, 54, 235};    // 슬라이더 트랙(빈 부분)
constexpr platform::Color kTrackDone {70, 110, 190, 235};  // 슬라이더 채워진 부분
constexpr float kLabelHeight = 30.0f;

// 한 버튼 외형(채움/외곽/가운데 라벨)을 그린다 — Menu와 Toolbar가 공유.
// enabled=false면 호버 강조를 무시하고 라벨을 회색으로 흐린다.
void DrawButton(platform::IRenderDevice& r, const math::Rect& rect,
                const std::string& label, bool highlight, float labelHeight,
                bool enabled = true) {
    r.FillRect(rect, (highlight && enabled) ? kBtnFillSel : kBtnFill);
    r.DrawRect(rect, kBtnBorder);
    DrawCenteredText(r, label, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f,
                     labelHeight, enabled ? kBtnText : kBtnTextOff);
}
} // namespace

void DrawCenteredText(platform::IRenderDevice& r, const std::string& text,
                      float cx, float cy, float pixelHeight, const platform::Color& color) {
    const math::Vector2D size = r.MeasureText(text, pixelHeight);
    r.DrawText(text, {cx - size.x * 0.5f, cy - size.y * 0.5f}, pixelHeight, color);
}

void Toolbar::LayoutRow(float startX, float y, float btnW, float btnH, float gap) {
    float x = startX;
    for (Item& it : m_items) {
        it.rect = math::Rect{x, y, btnW, btnH};
        x += btnW + gap;
    }
}

void Toolbar::LayoutColumn(float x, float startY, float btnW, float btnH, float gap) {
    float y = startY;
    for (Item& it : m_items) {
        it.rect = math::Rect{x, y, btnW, btnH};
        y += btnH + gap;
    }
}

void Toolbar::SetEnabled(int index, bool enabled) {
    if (index >= 0 && index < static_cast<int>(m_items.size()))
        m_items[index].enabled = enabled;
}

int Toolbar::Update(const platform::Input& in) {
    const math::Vector2D mouse = in.MousePos();
    m_hover = -1;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        if (m_items[i].enabled && m_items[i].rect.Contains(mouse)) { m_hover = i; break; }
    }
    if (m_hover != -1 && in.MousePressed(platform::MouseButton::Left)) return m_hover;
    return -1;
}

void Toolbar::Render(platform::IRenderDevice& r) const {
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        const Item& it = m_items[i];
        DrawButton(r, it.rect, it.label, i == m_hover || i == m_active, 20.0f, it.enabled);
    }
}

void Slider::Setup(float minVal, float maxVal, float value) {
    m_min = minVal;
    m_max = (maxVal > minVal) ? maxVal : minVal + 1.0f;
    SetValue(value);
}

void Slider::Layout(float x, float y, float w, float h) { m_rect = math::Rect{x, y, w, h}; }

void Slider::SetValue(float v) { m_value = std::clamp(v, m_min, m_max); }

bool Slider::Update(const platform::Input& in) {
    const math::Vector2D mouse = in.MousePos();
    m_hover = m_rect.Contains(mouse);
    const float prev = m_value;

    // 트랙 위에서 누르면 드래그 시작. 누른 지점으로 즉시 값이 점프(클릭=바로 이동).
    if (!m_dragging && m_hover && in.MousePressed(platform::MouseButton::Left)) m_dragging = true;

    if (m_dragging) {
        if (in.MouseDown(platform::MouseButton::Left)) {
            const float t = (m_rect.w > 0.0f) ? (mouse.x - m_rect.x) / m_rect.w : 0.0f;
            SetValue(m_min + std::clamp(t, 0.0f, 1.0f) * (m_max - m_min));
        } else {
            m_dragging = false; // 버튼 떼면 드래그 종료
        }
    }
    return m_value != prev;
}

void Slider::Render(platform::IRenderDevice& r) const {
    const float t = (m_max > m_min) ? (m_value - m_min) / (m_max - m_min) : 0.0f;
    const float handleX = m_rect.x + t * m_rect.w;

    r.FillRect(m_rect, kTrackFill);                                  // 트랙(빈 부분)
    r.FillRect({m_rect.x, m_rect.y, handleX - m_rect.x, m_rect.h}, kTrackDone); // 채워진 부분
    r.DrawRect(m_rect, kBtnBorder);

    // 핸들(트랙보다 살짝 큰 손잡이).
    constexpr float kHandleW = 10.0f;
    const math::Rect handle{handleX - kHandleW * 0.5f, m_rect.y - 3.0f, kHandleW, m_rect.h + 6.0f};
    r.FillRect(handle, (m_hover || m_dragging) ? kBtnFillSel : kBtnFill);
    r.DrawRect(handle, kBtnBorder);
}

void Dropdown::SetItemSize(float w, float h, float gap) {
    m_itemW = w; m_itemH = h; m_gap = gap;
    RelayoutItems();
}

void Dropdown::LayoutButton(float x, float y, float w, float h) {
    m_button = math::Rect{x, y, w, h};
    RelayoutItems();
}

void Dropdown::RelayoutItems() {
    // 헤더 바로 아래로 한 칸씩 쌓는다. 목록 폭은 헤더보다 넓을 수 있다(긴 라벨 대비).
    float y = m_button.Bottom() + 2.0f;
    for (Item& it : m_items) {
        it.rect = math::Rect{m_button.x, y, m_itemW, m_itemH};
        y += m_itemH + m_gap;
    }
}

Dropdown::Result Dropdown::Update(const platform::Input& in) {
    Result res;
    const math::Vector2D m = in.MousePos();
    const bool click = in.MousePressed(platform::MouseButton::Left);
    m_hover = -1;

    // 헤더 클릭 = 열고/닫기 토글.
    if (click && m_button.Contains(m)) { m_open = !m_open; res.toggled = true; return res; }

    if (m_open) {
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
            if (m_items[i].rect.Contains(m)) { m_hover = i; break; }
        if (click) {
            if (m_hover != -1) res.item = m_hover; // 항목 선택
            m_open = false;                        // 항목 선택이든 바깥 클릭이든 닫는다
        }
    }
    return res;
}

bool Dropdown::PointerOver(math::Vector2D p) const {
    if (m_button.Contains(p)) return true;
    if (!m_open) return false;
    for (const Item& it : m_items) if (it.rect.Contains(p)) return true;
    return false;
}

void Dropdown::RenderHeader(platform::IRenderDevice& r) const {
    DrawButton(r, m_button, m_label + "  ▼", m_open || m_active, 20.0f);
}

void Dropdown::RenderPopup(platform::IRenderDevice& r) const {
    if (!m_open) return;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        DrawButton(r, m_items[i].rect, m_items[i].label, i == m_hover, 19.0f);
}

bool SearchBox::Update(const platform::Input& in) {
    const math::Vector2D mouse = in.MousePos();
    m_hover = m_rect.Contains(mouse);
    if (in.MousePressed(platform::MouseButton::Left)) m_focused = m_hover; // 칸 안=포커스, 밖=해제

    if (!m_focused) return false;

    bool changed = false;
    const std::string& typed = in.TextInput();
    if (!typed.empty()) { m_text += typed; changed = true; }
    if (in.WasPressed(platform::Key::Backspace) && !m_text.empty()) {
        // UTF-8 한 글자 삭제: 끝의 후속바이트(10xxxxxx)들을 지우고 선두바이트까지 지운다.
        while (!m_text.empty()) {
            const unsigned char c = static_cast<unsigned char>(m_text.back());
            m_text.pop_back();
            if ((c & 0xC0) != 0x80) break; // ASCII이거나 선두바이트면 멈춘다
        }
        changed = true;
    }
    return changed;
}

void SearchBox::Render(platform::IRenderDevice& r) const {
    r.FillRect(m_rect, m_focused ? kTrackDone : kTrackFill);
    r.DrawRect(m_rect, m_focused ? kBtnFillSel : kBtnBorder);
    const bool empty = m_text.empty();
    const std::string shown = empty ? m_placeholder : m_text;
    if (!shown.empty()) {
        r.DrawText(shown + (m_focused ? "_" : ""), {m_rect.x + 8.0f, m_rect.y + m_rect.h * 0.5f - 9.0f},
                   18.0f, empty ? kBtnTextOff : kBtnText);
    } else if (m_focused) {
        r.DrawText("_", {m_rect.x + 8.0f, m_rect.y + m_rect.h * 0.5f - 9.0f}, 18.0f, kBtnText);
    }
}

void Menu::Layout(float centerX, float firstCenterY, float btnW, float btnH, float gap) {
    float cy = firstCenterY;
    for (Item& it : m_items) {
        it.rect = math::Rect{centerX - btnW * 0.5f, cy - btnH * 0.5f, btnW, btnH};
        cy += btnH + gap;
    }
}

int Menu::Update(const platform::Input& in) {
    if (m_items.empty()) return -1;

    const int count = static_cast<int>(m_items.size());

    // 마우스 호버 → 선택 이동.
    const math::Vector2D mouse = in.MousePos();
    int hovered = -1;
    for (int i = 0; i < count; ++i) {
        if (m_items[i].rect.Contains(mouse)) { hovered = i; break; }
    }
    if (hovered != -1) m_selected = hovered;

    // 키보드 위/아래 이동 (래핑).
    if (in.WasPressed(platform::Key::Down)) m_selected = (m_selected + 1) % count;
    if (in.WasPressed(platform::Key::Up))   m_selected = (m_selected - 1 + count) % count;

    // 활성화: 호버 중인 버튼 클릭 또는 Enter.
    if (hovered != -1 && in.MousePressed(platform::MouseButton::Left)) return hovered;
    if (in.WasPressed(platform::Key::Enter)) return m_selected;
    return -1;
}

void Menu::Render(platform::IRenderDevice& r) const {
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        const Item& it = m_items[i];
        DrawButton(r, it.rect, it.label, i == m_selected, kLabelHeight);
    }
}

} // namespace gs::app::ui
