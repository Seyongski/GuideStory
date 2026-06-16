#include "core/Ui.h"

namespace gs::app::ui {

namespace {
// 버튼 색 팔레트.
constexpr platform::Color kBtnFill   {45, 52, 74, 235};
constexpr platform::Color kBtnFillSel {70, 110, 190, 245}; // 호버/선택 강조
constexpr platform::Color kBtnBorder {120, 140, 180, 255};
constexpr platform::Color kBtnText   {235, 240, 250, 255};
constexpr float kLabelHeight = 30.0f;

// 한 버튼 외형(채움/외곽/가운데 라벨)을 그린다 — Menu와 Button이 공유.
void DrawButton(platform::IRenderDevice& r, const math::Rect& rect,
                const std::string& label, bool highlight, float labelHeight) {
    r.FillRect(rect, highlight ? kBtnFillSel : kBtnFill);
    r.DrawRect(rect, kBtnBorder);
    DrawCenteredText(r, label, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f,
                     labelHeight, kBtnText);
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

int Toolbar::Update(const platform::Input& in) {
    const math::Vector2D mouse = in.MousePos();
    m_hover = -1;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        if (m_items[i].rect.Contains(mouse)) { m_hover = i; break; }
    }
    if (m_hover != -1 && in.MousePressed(platform::MouseButton::Left)) return m_hover;
    return -1;
}

void Toolbar::Render(platform::IRenderDevice& r) const {
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        const Item& it = m_items[i];
        DrawButton(r, it.rect, it.label, i == m_hover || i == m_active, 20.0f);
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
