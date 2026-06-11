#include "MainMenuScreen.h"

#include <cstdio>

namespace gs::app {

namespace {
constexpr platform::Color kBg    {30, 34, 52, 255};
constexpr platform::Color kTitle {236, 224, 150, 255};

// 버튼 순서(인덱스)와 의미. Update의 switch와 1:1 대응.
enum MenuItem { kStart = 0, kOptions, kLogout, kQuit };
} // namespace

MainMenuScreen::MainMenuScreen() {
    m_menu.Add("게임 시작");
    m_menu.Add("환경 설정");
    m_menu.Add("로그아웃");
    m_menu.Add("게임 종료");
    // 화면 중앙 세로 배치.
    m_menu.Layout(kViewW * 0.5f, 300.0f, 320.0f, 64.0f, 24.0f);
}

SceneId MainMenuScreen::Update(const platform::Input& in, float /*dt*/) {
    switch (m_menu.Update(in)) {
        case kStart:   return SceneId::InGame;   // 추후: 캐릭터 선택창 경유
        case kLogout:  return SceneId::Login;
        case kQuit:    return SceneId::Quit;
        case kOptions: std::fprintf(stderr, "환경설정: 추후 구현\n"); break; // 1차 placeholder
        default:       break;
    }
    return SceneId::Stay;
}

void MainMenuScreen::Render(platform::IRenderDevice& r) {
    r.Clear(kBg);
    ui::DrawCenteredText(r, "GuideStory", kViewW * 0.5f, 160.0f, 64.0f, kTitle);
    m_menu.Render(r);
}

} // namespace gs::app
