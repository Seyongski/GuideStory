#include "core/ChatOverlay.h"

#include "net/Protocol.h"   // kMaxTextLen — 서버가 받는 한 줄 상한과 입력칸 상한을 맞춘다

namespace gs::core {

namespace {
constexpr platform::Color kPanelFill  {12, 14, 22, 170};   // 반투명 — 뒤 월드가 비쳐야 한다
constexpr platform::Color kPanelBorder{70, 82, 110, 200};

constexpr float kLineHeight = 18.0f;  // 글자 픽셀 높이
constexpr float kLineStep   = 21.0f;  // 줄 간격
constexpr float kPadding    = 8.0f;

// 화면에 유지하는 줄 수. 넘으면 오래된 것부터 버린다 — 로그 보관은 서버(chat_log)의 몫이고
// 클라이언트가 무한정 들고 있으면 장시간 플레이에서 메모리가 계속 는다.
constexpr std::size_t kMaxLines = 100;

// 패널에 한 번에 보이는 줄 수.
constexpr int kVisibleLines = 8;

// UTF-8 한 글자(선두 바이트 기준)의 바이트 수. 잘못된 바이트면 1로 본다(무한 루프 방지).
std::size_t Utf8CharLen(unsigned char c) {
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}
} // namespace

void ChatOverlay::Layout(float screenW, float screenH) {
    const float w = screenW * 0.42f;
    const float h = kVisibleLines * kLineStep + kPadding * 2.0f;
    const float x = 16.0f;

    // 아래에서부터 쌓는다: 입력줄이 맨 아래, 그 위가 로그 패널.
    const float inputH = 26.0f;
    const float inputY = screenH - 16.0f - inputH;
    m_inputBox = {x, inputY, w, inputH};
    m_panel    = {x, inputY - 6.0f - h, w, h};

    m_input.Layout(m_inputBox.x, m_inputBox.y, m_inputBox.w, m_inputBox.h);
    m_input.SetPlaceholder("Enter 로 채팅  ·  /w 닉네임 내용 = 귓속말");
    m_input.SetMaxBytes(net::kMaxTextLen);
    m_dirty = true;
}

void ChatOverlay::Add(std::string text, const platform::Color& color) {
    m_lines.push_back({std::move(text), color});
    while (m_lines.size() > kMaxLines) m_lines.pop_front();
    m_dirty = true;
}

bool ChatOverlay::Update(const platform::Input& in, std::string& outSubmitted) {
    if (!m_active) {
        // 닫혀 있을 때는 Enter 만 본다. 이동/점프 키는 GameScreen 이 그대로 쓴다.
        if (in.WasPressed(platform::Key::Enter)) {
            m_active = true;
            m_input.SetFocused(true);
        }
        return false;
    }

    const app::ui::TextField::Result r = m_input.Update(in);

    // 입력칸 바깥을 클릭하면 TextField 가 스스로 포커스를 놓는다. 그 상태로 열어두면
    // 칸은 보이는데 타이핑도 Enter 도 먹지 않는 "죽은 입력줄" 이 된다. 그럴 땐 닫는다.
    if (!m_input.Focused()) {
        m_active = false;
        return false;
    }

    if (!r.submitted) return false;

    std::string text = m_input.Text();
    m_input.Clear();

    if (text.empty()) {          // 빈 줄에서 Enter = 닫기 (ESC 를 쓰지 않는 이유는 헤더 주석)
        m_active = false;
        m_input.SetFocused(false);
        return false;
    }

    outSubmitted = std::move(text);
    return true;
}

void ChatOverlay::RebuildWrap(platform::IRenderDevice& r) {
    m_wrapped.clear();
    const float maxW = m_panel.w - kPadding * 2.0f;

    for (const Entry& entry : m_lines) {
        // 흔한 경우(짧은 줄)는 측정 한 번으로 끝난다. 접기는 넘칠 때만 한다.
        if (r.MeasureText(entry.text, kLineHeight).x <= maxW) {
            m_wrapped.push_back(entry);
            continue;
        }

        std::string current;
        std::size_t i = 0;
        while (i < entry.text.size()) {
            const std::size_t len =
                Utf8CharLen(static_cast<unsigned char>(entry.text[i]));
            const std::string next = current + entry.text.substr(i, len);

            if (!current.empty() && r.MeasureText(next, kLineHeight).x > maxW) {
                m_wrapped.push_back({current, entry.color});
                current.clear();
                continue;   // 같은 글자를 새 줄에 다시 넣는다
            }
            current = next;
            i += len;
        }
        if (!current.empty()) m_wrapped.push_back({current, entry.color});
    }
    m_dirty = false;
}

void ChatOverlay::Render(platform::IRenderDevice& r) {
    if (m_dirty) RebuildWrap(r);

    r.FillRect(m_panel, kPanelFill);
    r.DrawRect(m_panel, kPanelBorder);

    // 아래에서 위로 최근 줄부터 채운다. 위가 남으면 비워둔다(오래된 줄이 위로 밀려 사라짐).
    float y = m_panel.Bottom() - kPadding - kLineStep;
    for (int i = static_cast<int>(m_wrapped.size()) - 1;
         i >= 0 && y >= m_panel.y + kPadding - 1.0f; --i) {
        const Entry& line = m_wrapped[static_cast<std::size_t>(i)];
        r.DrawText(line.text, {m_panel.x + kPadding, y}, kLineHeight, line.color);
        y -= kLineStep;
    }

    // 입력줄은 열려 있을 때만 그린다 — 평소에 화면 아래를 가리지 않게.
    if (m_active) m_input.Render(r);
}

} // namespace gs::core
