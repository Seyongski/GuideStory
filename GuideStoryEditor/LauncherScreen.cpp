#include "LauncherScreen.h"

namespace gs::app {

namespace {
constexpr platform::Color kBg    {30, 34, 52, 255};
constexpr platform::Color kTitle {236, 224, 150, 255};
constexpr platform::Color kHint  {150, 160, 180, 255};

// 버튼 순서(인덱스)와 의미. Update의 switch와 1:1 대응.
enum LauncherItem { kMap = 0, kPlayer, kSkill, kMonster, kNpc };
} // namespace

LauncherScreen::LauncherScreen() {
    m_menu.Add("맵 에디터");
    m_menu.Add("플레이어 에디터");
    m_menu.Add("스킬 에디터");
    m_menu.Add("몬스터 에디터");
    m_menu.Add("NPC 에디터");
    m_menu.Layout(kViewW * 0.5f, 260.0f, 360.0f, 60.0f, 18.0f);
}

EditorScene LauncherScreen::Update(const platform::Input& in, float /*dt*/) {
    if (in.WasPressed(platform::Key::Escape)) return EditorScene::Quit;
    switch (m_menu.Update(in)) {
        case kMap:     return EditorScene::MapEditor;
        case kPlayer:  return EditorScene::PlayerEditor;
        case kSkill:   return EditorScene::SkillEditor;
        case kMonster: return EditorScene::MonsterEditor;
        case kNpc:     return EditorScene::NpcEditor;
        default:       return EditorScene::Stay;
    }
}

void LauncherScreen::Render(platform::IRenderDevice& r) {
    r.Clear(kBg);
    ui::DrawCenteredText(r, "GuideStory 에디터", kViewW * 0.5f, 150.0f, 56.0f, kTitle);
    m_menu.Render(r);
    ui::DrawCenteredText(r, "편집할 대상을 선택하세요  ·  ESC 종료",
                         kViewW * 0.5f, kViewH - 60.0f, 20.0f, kHint);
}

} // namespace gs::app
