#include "core/KeySettingOverlay.h"

namespace gs::core {

using platform::Key;
using platform::MouseButton;
using math::Rect;
using math::Vector2D;

namespace {
// 색 팔레트(메이플식 다크 패널).
constexpr platform::Color kDim       {0, 0, 0, 150};          // 화면 어둡게
constexpr platform::Color kPanelFill {28, 32, 48, 248};
constexpr platform::Color kPanelEdge {120, 140, 180, 255};
constexpr platform::Color kCellFill  {45, 52, 74, 255};
constexpr platform::Color kCellHot   {70, 110, 190, 255};      // 드롭 대상 강조
constexpr platform::Color kCellEdge  {120, 140, 180, 255};
constexpr platform::Color kChipFill  {196, 140, 60, 255};      // 행동 칩(주황)
constexpr platform::Color kChipEdge  {235, 200, 120, 255};
constexpr platform::Color kText      {235, 240, 250, 255};
constexpr platform::Color kTextDim   {150, 160, 180, 255};
constexpr platform::Color kKeyName   {180, 205, 245, 255};
constexpr platform::Color kDlgFill   {38, 44, 64, 252};       // 확인창 박스
constexpr platform::Color kDlgEdge   {150, 175, 220, 255};

// 하단 버튼 인덱스.
enum Btn { kBtnSave = 0, kBtnRevert, kBtnReset, kBtnClose };

// 행동 칩 외형(채움/외곽/라벨).
void DrawChip(platform::IRenderDevice& r, const Rect& box, std::string_view label, bool dim) {
    r.FillRect(box, dim ? platform::Color{90, 78, 52, 255} : kChipFill);
    r.DrawRect(box, kChipEdge);
    app::ui::DrawCenteredText(r, std::string(label), box.x + box.w * 0.5f, box.y + box.h * 0.5f,
                         22.0f, dim ? kTextDim : kText);
}
} // namespace

KeySettingOverlay::KeySettingOverlay(InputMap& bindings) : m_bindings(bindings) {
    m_buttons.Add("저장하기"); // kBtnSave
    m_buttons.Add("원래대로"); // kBtnRevert
    m_buttons.Add("초기화");   // kBtnReset
    m_buttons.Add("닫기");     // kBtnClose
    m_confirmButtons.Add("확인");
    m_confirmButtons.Add("취소");
}

void KeySettingOverlay::Open(float screenW, float screenH) {
    m_screenW = screenW;
    m_screenH = screenH;
    m_carried.reset();
    m_carriedFrom.reset();
    m_confirmClose = false;
    m_baseline = m_bindings; // 열 때의 상태(=마지막 저장본)를 기준으로 변동 비교

    // 화면 중앙 패널.
    const float pw = 760.0f, ph = 470.0f;
    const float x0 = (screenW - pw) * 0.5f;
    const float y0 = (screenH - ph) * 0.5f;
    m_panel = {x0, y0, pw, ph};

    // 키 칸 한 줄(7개).
    const float cellW = 96.0f, cellH = 92.0f, cellGap = 8.0f;
    const float keyRowW = kKeyCount * cellW + (kKeyCount - 1) * cellGap;
    float kx = x0 + (pw - keyRowW) * 0.5f;
    const float ky = y0 + 95.0f;
    for (std::size_t i = 0; i < kKeyCount; ++i) {
        m_keyCell[i] = {kx, ky, cellW, cellH};
        kx += cellW + cellGap;
    }

    // 행동 트레이 슬롯(왼쪽부터 채움). 미할당 행동만 여기에 표시된다.
    const float chipW = 120.0f, chipH = 56.0f, chipGap = 14.0f;
    const float trayW = kActionCount * chipW + (kActionCount - 1) * chipGap;
    float cx = x0 + (pw - trayW) * 0.5f;
    const float cy = y0 + 250.0f;
    for (std::size_t i = 0; i < kActionCount; ++i) {
        m_chipSlot[i] = {cx, cy, chipW, chipH};
        cx += chipW + chipGap;
    }

    // 하단 버튼 4개(저장하기/원래대로/초기화/닫기).
    const float btnW = 120.0f, btnH = 46.0f, btnGap = 14.0f;
    const float barW = 4 * btnW + 3 * btnGap;
    m_buttons.LayoutRow(x0 + (pw - barW) * 0.5f, y0 + ph - 70.0f, btnW, btnH, btnGap);
    m_buttons.SetEnabled(kBtnSave, false);   // 막 열었을 땐 변동 없음
    m_buttons.SetEnabled(kBtnRevert, false);

    // 닫기 확인창(화면 중앙) + 확인/취소 버튼.
    const float dw = 440.0f, dh = 180.0f;
    m_confirmBox = {(screenW - dw) * 0.5f, (screenH - dh) * 0.5f, dw, dh};
    const float cbW = 110.0f, cbH = 44.0f, cbGap = 24.0f;
    const float cbBar = 2 * cbW + cbGap;
    m_confirmButtons.LayoutRow(m_confirmBox.x + (dw - cbBar) * 0.5f,
                               m_confirmBox.y + dh - 62.0f, cbW, cbH, cbGap);

    m_visible = true;
}

void KeySettingOverlay::CancelCarry() {
    if (!m_carried) return;
    // 집어 든 원래 키가 비어 있으면 거기로 되돌리고, 아니면 트레이(미할당)로 남긴다.
    if (m_carriedFrom && !m_bindings.ActionForKey(*m_carriedFrom).has_value())
        m_bindings.Bind(*m_carried, *m_carriedFrom);
    m_carried.reset();
    m_carriedFrom.reset();
}

bool KeySettingOverlay::IsDirty() const {
    for (std::size_t i = 0; i < kActionCount; ++i) {
        const auto a = static_cast<Action>(i);
        if (m_bindings.KeyFor(a) != m_baseline.KeyFor(a)) return true;
    }
    return false;
}

void KeySettingOverlay::CommitBaseline() { m_baseline = m_bindings; }

void KeySettingOverlay::RevertToBaseline() {
    m_bindings = m_baseline; // 참조 대상(실 바인딩)에 baseline을 복사 — 게임에 즉시 반영
    m_carried.reset();
    m_carriedFrom.reset();
}

void KeySettingOverlay::RequestClose() {
    if (m_confirmClose) {        // 확인창에서 \·ESC = 취소(되돌리고 종료)
        RevertToBaseline();
        Close();
        return;
    }
    CancelCarry();
    if (IsDirty()) m_confirmClose = true; // 변동 있음 → 확인창
    else           Close();
}

std::vector<Action> KeySettingOverlay::TrayActions() const {
    std::vector<Action> out;
    for (std::size_t i = 0; i < kActionCount; ++i) {
        const auto a = static_cast<Action>(i);
        const bool unbound = !m_bindings.KeyFor(a).has_value();
        const bool carried = m_carried && *m_carried == a;
        if (unbound && !carried) out.push_back(a); // 미할당이고 들고 있지도 않은 것만
    }
    return out;
}

KeySettingOverlay::Result KeySettingOverlay::Update(const platform::Input& in) {
    Result result;
    if (!m_visible) return result;

    const Vector2D mouse = in.MousePos();
    m_cursorPos = mouse;

    // 닫기 확인창이 떠 있으면 모달 — 확인/취소만 처리한다.
    if (m_confirmClose) {
        switch (m_confirmButtons.Update(in)) {
            case 0: // 확인 = 저장 후 종료
                CommitBaseline();
                result.saveRequested = true;
                Close();
                break;
            case 1: // 취소 = 되돌리고 종료
                RevertToBaseline();
                Close();
                break;
            default: break;
        }
        return result;
    }

    // 변동사항 여부에 따라 저장하기·원래대로 버튼 활성/비활성.
    // 단, 슬롯을 들고 있는 중(carry)에는 해당 행동이 잠시 미할당이라 가짜 변동으로 잡히므로
    // 평가하지 않고 직전 상태를 유지한다 — 배치가 끝난 뒤에만 실제 변동을 반영한다.
    if (!m_carried) {
        const bool dirty = IsDirty();
        m_buttons.SetEnabled(kBtnSave, dirty);
        m_buttons.SetEnabled(kBtnRevert, dirty);
    }

    // 하단 버튼.
    switch (m_buttons.Update(in)) {
        case kBtnSave:                                              // 저장하기(변동 시만 활성)
            CancelCarry(); CommitBaseline(); result.saveRequested = true; return result;
        case kBtnRevert:                                            // 원래대로(직전 저장본 복귀)
            RevertToBaseline();                                     return result;
        case kBtnReset:                                             // 초기화(기본값)
            CancelCarry(); m_bindings.ResetDefaults();              return result;
        case kBtnClose:                                             // 닫기(변동 시 확인창)
            CancelCarry();
            if (IsDirty()) m_confirmClose = true; else Close();
            return result;
        default: break;
    }

    // 어느 키 칸 위인가(없으면 -1).
    int cell = -1;
    for (std::size_t i = 0; i < kKeyCount; ++i)
        if (m_keyCell[i].Contains(mouse)) { cell = static_cast<int>(i); break; }

    // 우클릭: 들고 있으면 취소(원래 자리로 복귀), 아니면 그 칸의 매핑을 해제.
    if (in.MousePressed(MouseButton::Right)) {
        if (m_carried) {
            CancelCarry();
        } else if (cell != -1) {
            if (auto a = m_bindings.ActionForKey(InputMap::AssignableKeys()[cell]))
                m_bindings.Unbind(*a);
        }
        return result;
    }

    // 좌클릭: 집기 / 놓기 / 스왑 / 취소.
    if (in.MousePressed(MouseButton::Left)) {
        if (m_carried) {
            if (cell != -1) {
                // 키 칸에 놓는다. 점유 중이면 기존 행동이 밀려나 새로 들린다(스왑).
                const Key key = InputMap::AssignableKeys()[cell];
                const auto displaced = m_bindings.ActionForKey(key);
                if (displaced) m_bindings.Unbind(*displaced);
                m_bindings.Bind(*m_carried, key);
                m_carried = displaced;     // 밀려난 행동이 있으면 그게 들리고, 없으면 놓임
                m_carriedFrom.reset();     // 스왑으로 들린 행동은 되돌릴 원래 자리가 없다
            } else {
                // 키 칸이 아닌 곳 클릭: 이동 취소 → 원래 매핑 자리로 복귀.
                CancelCarry();
            }
        } else {
            // 아무것도 안 들고 있을 때: 매핑된 칸이면 집어 들고, 트레이 칩이면 집는다.
            if (cell != -1) {
                const Key key = InputMap::AssignableKeys()[cell];
                if (auto a = m_bindings.ActionForKey(key)) {
                    m_bindings.Unbind(*a);
                    m_carried = a;
                    m_carriedFrom = key;   // 취소 시 이 키로 되돌린다
                }
            } else {
                const std::vector<Action> tray = TrayActions();
                for (std::size_t i = 0; i < tray.size(); ++i)
                    if (m_chipSlot[i].Contains(mouse)) {
                        m_carried = tray[i];
                        m_carriedFrom.reset(); // 트레이에서 집음 → 취소 시 트레이로
                        break;
                    }
            }
        }
    }

    return result;
}

void KeySettingOverlay::Render(platform::IRenderDevice& r) const {
    if (!m_visible) return;

    r.FillRect({0.0f, 0.0f, m_screenW, m_screenH}, kDim);
    r.FillRect(m_panel, kPanelFill);
    r.DrawRect(m_panel, kPanelEdge);

    const float cx = m_panel.x + m_panel.w * 0.5f;
    app::ui::DrawCenteredText(r, "키보드 설정", cx, m_panel.y + 30.0f, 30.0f, kText);
    app::ui::DrawCenteredText(r, "행동을 클릭해 집고 키 칸을 클릭해 지정 · 우클릭으로 해제/취소 · \\ 또는 ESC로 닫기",
                         cx, m_panel.y + 62.0f, 17.0f, kTextDim);

    // 키 칸: 위=키 이름, 아래=바인딩된 행동. 들고 있는 동안 마우스가 올라간 칸을 강조.
    const Vector2D mouse = m_cursorPos;
    for (std::size_t i = 0; i < kKeyCount; ++i) {
        const Rect& cell = m_keyCell[i];
        const Key key = InputMap::AssignableKeys()[i];
        const bool hot = m_carried && cell.Contains(mouse); // 놓을 대상 강조
        r.FillRect(cell, hot ? kCellHot : kCellFill);
        r.DrawRect(cell, kCellEdge);

        app::ui::DrawCenteredText(r, std::string(InputMap::KeyLabel(key)),
                             cell.x + cell.w * 0.5f, cell.y + 24.0f, 22.0f, kKeyName);
        r.DrawLine({cell.x + 8.0f, cell.y + 44.0f}, {cell.Right() - 8.0f, cell.y + 44.0f}, kCellEdge);

        if (auto a = m_bindings.ActionForKey(key)) {
            app::ui::DrawCenteredText(r, std::string(InputMap::Label(*a)),
                                 cell.x + cell.w * 0.5f, cell.y + 68.0f, 20.0f, kText);
        } else {
            app::ui::DrawCenteredText(r, "비어있음",
                                 cell.x + cell.w * 0.5f, cell.y + 68.0f, 16.0f, kTextDim);
        }
    }

    // 행동 트레이: 미할당(또한 안 들고 있는) 행동만 왼쪽부터 채워 표시.
    app::ui::DrawCenteredText(r, "행동", m_panel.x + 36.0f, m_chipSlot[0].y - 18.0f, 18.0f, kTextDim);
    const std::vector<Action> tray = TrayActions();
    for (std::size_t i = 0; i < tray.size(); ++i)
        DrawChip(r, m_chipSlot[i], InputMap::Label(tray[i]), false);

    m_buttons.Render(r);

    // 집어 든 행동은 커서를 따라다닌다.
    // 커서는 칩 위에 올라가되, 칩 정중앙에서 살짝 오른쪽-아래 지점에 오도록 칩을 배치한다
    // (= 칩 중심을 커서의 왼쪽-위로 약간 옮김). 그러면 마우스 포인터가 칩에 가리지 않는다.
    if (m_carried) {
        const float gw = 120.0f, gh = 44.0f;
        const float dx = 14.0f, dy = 10.0f; // 커서가 칩 중심에서 벗어나는 정도(오른쪽-아래)
        DrawChip(r, {mouse.x - gw * 0.5f - dx, mouse.y - gh * 0.5f - dy, gw, gh},
                 InputMap::Label(*m_carried), false);
    }

    // 닫기 확인창(모달) — 가장 위에 덧그린다.
    if (m_confirmClose) {
        r.FillRect({0.0f, 0.0f, m_screenW, m_screenH}, kDim);
        r.FillRect(m_confirmBox, kDlgFill);
        r.DrawRect(m_confirmBox, kDlgEdge);
        const float dcx = m_confirmBox.x + m_confirmBox.w * 0.5f;
        app::ui::DrawCenteredText(r, "변동사항이 있습니다. 저장하시겠습니까?",
                                  dcx, m_confirmBox.y + 56.0f, 20.0f, kText);
        m_confirmButtons.Render(r);
    }
}

} // namespace gs::core
