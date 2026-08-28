#include "MainMenuScreen.h"

#include <cstdio>

namespace gs::app {

namespace {
constexpr platform::Color kBg     {30, 34, 52, 255};
constexpr platform::Color kTitle  {236, 224, 150, 255};
constexpr platform::Color kWho    {170, 200, 240, 255};
constexpr platform::Color kNotice {235, 170, 110, 255};

// 버튼 순서(인덱스)와 의미. Update의 switch와 1:1 대응.
enum MenuItem { kStart = 0, kOptions, kLogout, kQuit };
} // namespace

MainMenuScreen::MainMenuScreen(net::NetClient& net) : m_net(net) {
    m_menu.Add("게임 시작");
    m_menu.Add("환경 설정");
    m_menu.Add("로그아웃");
    m_menu.Add("게임 종료");
    // 화면 중앙 세로 배치.
    m_menu.Layout(kViewW * 0.5f, 320.0f, 320.0f, 64.0f, 24.0f);
}

SceneId MainMenuScreen::Update(const platform::Input& in, float /*dt*/) {
    switch (m_menu.Update(in)) {
        case kStart:   return SceneId::InGame;   // 추후: 캐릭터 선택창 경유
        case kLogout:  return SceneId::Login;    // App이 전환하면서 NetClient::Logout을 부른다
        case kQuit:    return SceneId::Quit;
        case kOptions: std::fprintf(stderr, "환경설정: 추후 구현\n"); break; // 1차 placeholder
        default:       break;
    }
    return SceneId::Stay;
}

void MainMenuScreen::OnNetEvent(const net::NetEvent& ev) {
    switch (ev.type) {
        case net::NetEventType::Disconnected:
        case net::NetEventType::ConnectFailed:
            // 로그인 화면으로 되돌리지 않는다 — 워커가 저장된 자격으로 자동 재로그인하므로
            // 잠깐 끊긴 것만으로 사용자를 로그인 창까지 내보내면 오히려 성가시다.
            m_notice = "서버와 연결이 끊어졌습니다. 다시 연결 중…";
            break;
        case net::NetEventType::LoginAck:
            if (ev.success) m_notice.clear();
            break;
        default:
            break;
    }
}

void MainMenuScreen::Render(platform::IRenderDevice& r) {
    r.Clear(kBg);
    ui::DrawCenteredText(r, "GuideStory", kViewW * 0.5f, 150.0f, 64.0f, kTitle);

    // 누구로 로그인했는지 항상 보이게 한다. 계정이 여러 개일 때 "지금 누구지?" 를 없앤다.
    if (m_net.LoggedIn()) {
        ui::DrawCenteredText(r, m_net.Nickname() + " 님, 환영합니다",
                             kViewW * 0.5f, 225.0f, 24.0f, kWho);
    }

    m_menu.Render(r);

    if (!m_notice.empty()) {
        ui::DrawCenteredText(r, m_notice, kViewW * 0.5f, kViewH - 50.0f, 20.0f, kNotice);
    }
}

} // namespace gs::app
