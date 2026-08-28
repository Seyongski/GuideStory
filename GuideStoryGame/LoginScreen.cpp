#include "LoginScreen.h"

#include <cstdio>

namespace gs::app {

namespace {
constexpr platform::Color kBg      {26, 28, 38, 255};
constexpr platform::Color kTitle   {236, 224, 150, 255};
constexpr platform::Color kLabel   {200, 208, 224, 255};
constexpr platform::Color kHint    {150, 160, 180, 255};
constexpr platform::Color kError   {235, 120, 120, 255};
constexpr platform::Color kOk      {140, 210, 150, 255};

constexpr float kBoxW = 360.0f;
constexpr float kBoxH = 44.0f;
constexpr float kFirstY = 300.0f;   // 첫 입력 칸의 y
constexpr float kRowStep = 62.0f;

float BoxX() { return kViewW * 0.5f - kBoxW * 0.5f; }
float RowY(int index) { return kFirstY + kRowStep * static_cast<float>(index); }
} // namespace

LoginScreen::LoginScreen(net::NetClient& net) : m_net(net) {
    m_id.SetPlaceholder("아이디");
    m_id.SetMaxBytes(net::kMaxLoginIdLen - 1);      // 고정 배열은 널 종료를 포함한다

    m_password.SetPlaceholder("비밀번호");
    m_password.SetPassword(true);
    m_password.SetMaxBytes(net::kMaxPasswordLen - 1);

    m_nickname.SetPlaceholder("닉네임 (게임에서 보이는 이름)");
    m_nickname.SetMaxBytes(net::kMaxNameLen - 1);

    SetMode(Mode::Login);
    m_id.SetFocused(true);   // 창을 열면 바로 타이핑할 수 있게
}

void LoginScreen::SetMode(Mode mode) {
    m_mode = mode;
    m_focus = 0;
    m_waiting = false;

    m_id.Layout(BoxX(), RowY(0), kBoxW, kBoxH);
    m_password.Layout(BoxX(), RowY(1), kBoxW, kBoxH);
    m_nickname.Layout(BoxX(), RowY(2), kBoxW, kBoxH);

    m_buttons.Clear();
    if (m_mode == Mode::Login) {
        m_buttons.Add("로그인");
        m_buttons.Add("회원가입");
    } else {
        m_buttons.Add("가입하기");
        m_buttons.Add("돌아가기");
        m_nickname.Clear();
    }
    // 마지막 입력 칸 아래로 버튼 두 개를 나란히.
    const float btnW = (kBoxW - 12.0f) * 0.5f;
    m_buttons.LayoutRow(BoxX(), RowY(FieldCount()) + 8.0f, btnW, kBoxH, 12.0f);

    m_id.SetFocused(true);
    m_password.SetFocused(false);
    m_nickname.SetFocused(false);
}

void LoginScreen::FocusNext() {
    m_focus = (m_focus + 1) % FieldCount();
    m_id.SetFocused(m_focus == 0);
    m_password.SetFocused(m_focus == 1);
    m_nickname.SetFocused(m_focus == 2);
}

void LoginScreen::Submit() {
    if (m_waiting) return;   // 응답을 기다리는 중이면 같은 요청을 또 보내지 않는다

    if (!m_net.Connected()) {
        m_message = "서버에 연결되어 있지 않습니다. 연결될 때까지 기다려 주세요.";
        m_messageIsError = true;
        return;
    }

    // 클라이언트 검사는 왕복을 아끼기 위한 편의일 뿐이다 — 판정은 언제나 서버가 한다.
    if (!net::NetClient::IsLoginIdValid(m_id.Text())) {
        m_message = "아이디는 " + std::to_string(net::kMinLoginIdLen) + "자 이상이어야 합니다.";
        m_messageIsError = true;
        return;
    }
    if (!net::NetClient::IsPasswordValid(m_password.Text())) {
        m_message = "비밀번호는 " + std::to_string(net::kMinPasswordLen) + "자 이상이어야 합니다.";
        m_messageIsError = true;
        return;
    }

    if (m_mode == Mode::Login) {
        m_waiting = m_net.RequestLogin(m_id.Text(), m_password.Text());
        m_message = "로그인 중…";
        m_messageIsError = false;
        return;
    }

    if (!net::NetClient::IsNicknameValid(m_nickname.Text())) {
        m_message = "닉네임은 " + std::to_string(net::kMinNicknameLen) + "자 이상이어야 합니다.";
        m_messageIsError = true;
        return;
    }
    m_waiting = m_net.RequestRegister(m_id.Text(), m_password.Text(), m_nickname.Text());
    m_message = "가입 요청 중…";
    m_messageIsError = false;
}

SceneId LoginScreen::Update(const platform::Input& in, float /*dt*/) {
    if (m_pending != SceneId::Stay) {
        const SceneId next = m_pending;
        m_pending = SceneId::Stay;
        return next;
    }

    if (in.WasPressed(platform::Key::Tab)) FocusNext();

    // 칸들을 모두 갱신한다. 포커스는 각 칸이 마우스 클릭으로 스스로 판단하므로,
    // 그 결과를 읽어 m_focus(Tab 순회의 기준)를 다시 맞춘다.
    bool submitted = false;
    submitted |= m_id.Update(in).submitted;
    submitted |= m_password.Update(in).submitted;
    if (m_mode == Mode::Register) submitted |= m_nickname.Update(in).submitted;

    if (m_id.Focused())            m_focus = 0;
    else if (m_password.Focused()) m_focus = 1;
    else if (m_nickname.Focused()) m_focus = 2;

    const bool anyFocused = m_id.Focused() || m_password.Focused() ||
                            (m_mode == Mode::Register && m_nickname.Focused());
    // 아무 칸도 포커스가 없을 때의 Enter 도 제출로 받는다(칸 밖을 클릭한 뒤에도 동작하도록).
    if (!anyFocused && in.WasPressed(platform::Key::Enter)) submitted = true;

    if (submitted) Submit();

    switch (m_buttons.Update(in)) {
        case 0: Submit(); break;
        case 1:
            SetMode(m_mode == Mode::Login ? Mode::Register : Mode::Login);
            m_message.clear();
            break;
        default: break;
    }

    return SceneId::Stay;
}

void LoginScreen::OnNetEvent(const net::NetEvent& ev) {
    switch (ev.type) {
        case net::NetEventType::Connecting:
            if (!m_waiting) { m_message = "서버에 연결 중…"; m_messageIsError = false; }
            break;

        case net::NetEventType::Connected:
            m_message = "서버에 연결되었습니다.";
            m_messageIsError = false;
            break;

        case net::NetEventType::ConnectFailed:
            m_message = ev.text;
            m_messageIsError = true;
            m_waiting = false;
            break;

        case net::NetEventType::Disconnected:
            m_message = "서버와 연결이 끊어졌습니다. 잠시 후 다시 시도합니다.";
            m_messageIsError = true;
            m_waiting = false;
            break;

        case net::NetEventType::LoginAck:
            m_waiting = false;
            if (ev.success) {
                m_password.Clear();   // 화면에 남겨둘 이유가 없다
                m_pending = SceneId::MainMenu;
            } else {
                m_message = net::LoginResultText(ev.result);
                m_messageIsError = true;
            }
            break;

        case net::NetEventType::RegisterAck:
            m_waiting = false;
            if (ev.success) {
                // 자동 로그인은 시키지 않는다(서버도 그렇게 나눠 두었다).
                // 방금 만든 계정으로 한 번 더 로그인해 보는 것이 확인이 된다.
                SetMode(Mode::Login);
                m_message = "가입이 완료되었습니다. 이제 로그인하세요.";
                m_messageIsError = false;
            } else {
                m_message = net::LoginResultText(ev.result);
                m_messageIsError = true;
            }
            break;

        default:
            break;   // 채팅은 로그인 화면에서 보여줄 자리가 없다
    }
}

void LoginScreen::Render(platform::IRenderDevice& r) {
    r.Clear(kBg);

    ui::DrawCenteredText(r, "GuideStory", kViewW * 0.5f, 150.0f, 64.0f, kTitle);
    ui::DrawCenteredText(r, m_mode == Mode::Login ? "로그인" : "회원가입",
                         kViewW * 0.5f, 235.0f, 30.0f, kLabel);

    // 칸 왼쪽에 라벨. 칸 자체의 placeholder 와 중복되지 않게 짧게 둔다.
    const float labelX = BoxX() - 20.0f;
    ui::DrawCenteredText(r, "아이디",   labelX - 30.0f, RowY(0) + kBoxH * 0.5f, 22.0f, kLabel);
    ui::DrawCenteredText(r, "비밀번호", labelX - 30.0f, RowY(1) + kBoxH * 0.5f, 22.0f, kLabel);
    m_id.Render(r);
    m_password.Render(r);
    if (m_mode == Mode::Register) {
        ui::DrawCenteredText(r, "닉네임", labelX - 30.0f, RowY(2) + kBoxH * 0.5f, 22.0f, kLabel);
        m_nickname.Render(r);
    }

    m_buttons.Render(r);

    // 상태 한 줄: 마지막 결과 또는 접속 상태. 화면과 서버 콘솔이 같은 문장을 쓰게 해서
    // "화면에는 실패만 뜨고 원인은 서버 로그에만 있는" 상황을 막는다.
    if (!m_message.empty()) {
        ui::DrawCenteredText(r, m_message, kViewW * 0.5f, RowY(FieldCount()) + 78.0f, 20.0f,
                             m_messageIsError ? kError : kOk);
    }

    char serverText[128];
    std::snprintf(serverText, sizeof(serverText), "서버 %s:%u  ·  %s",
                  m_net.Host().c_str(), static_cast<unsigned>(m_net.Port()),
                  m_net.Connected() ? "연결됨" : "연결 안 됨");
    ui::DrawCenteredText(r, serverText, kViewW * 0.5f, kViewH - 60.0f, 18.0f, kHint);
    ui::DrawCenteredText(r, "Tab 다음 칸  ·  Enter 확인  ·  ESC 종료",
                         kViewW * 0.5f, kViewH - 34.0f, 18.0f, kHint);
}

} // namespace gs::app
