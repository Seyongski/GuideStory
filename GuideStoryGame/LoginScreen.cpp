#include "LoginScreen.h"

#include "Ui.h"

namespace gs::app {

namespace {
constexpr platform::Color kBg     {26, 28, 38, 255};
constexpr platform::Color kTitle  {236, 224, 150, 255};
constexpr platform::Color kLabel  {200, 208, 224, 255};
constexpr platform::Color kField  {45, 52, 74, 255};
constexpr platform::Color kBorder {120, 140, 180, 255};
constexpr platform::Color kHint   {150, 160, 180, 255};

// 입력 칸 한 줄(라벨 + 빈 입력 박스)을 그린다. 1차에서는 입력 기능 없이 외형만.
void DrawField(platform::IRenderDevice& r, const std::string& label, float cy) {
    const float boxW = 360.0f, boxH = 48.0f;
    const float boxX = kViewW * 0.5f - boxW * 0.5f;
    ui::DrawCenteredText(r, label, boxX - 60.0f, cy + boxH * 0.5f, 24.0f, kLabel);
    const math::Rect box{boxX, cy, boxW, boxH};
    r.FillRect(box, kField);
    r.DrawRect(box, kBorder);
}
} // namespace

SceneId LoginScreen::Update(const platform::Input& in, float /*dt*/) {
    // 1차: 실제 인증 없이 Enter 또는 클릭으로 메인화면 진입.
    if (in.WasPressed(platform::Key::Enter) ||
        in.WasPressed(platform::Key::Space) ||
        in.MousePressed(platform::MouseButton::Left)) {
        return SceneId::MainMenu;
    }
    return SceneId::Stay;
}

void LoginScreen::Render(platform::IRenderDevice& r) {
    r.Clear(kBg);

    ui::DrawCenteredText(r, "GuideStory", kViewW * 0.5f, 150.0f, 64.0f, kTitle);
    ui::DrawCenteredText(r, "로그인", kViewW * 0.5f, 240.0f, 30.0f, kLabel);

    DrawField(r, "아이디",     320.0f);
    DrawField(r, "비밀번호",   400.0f);

    ui::DrawCenteredText(r, "Enter 또는 클릭하여 로그인  ·  ESC 종료",
                         kViewW * 0.5f, 520.0f, 22.0f, kHint);
    ui::DrawCenteredText(r, "(계정 생성 / 찾기 기능은 추후 추가)",
                         kViewW * 0.5f, 560.0f, 18.0f, kHint);
}

} // namespace gs::app
