#pragma once

#include "EditorScreen.h"

#include "core/Ui.h"

namespace gs::app {

// 에디터 선택 화면. 맵/플레이어/스킬/몬스터/NPC 에디터 중 하나로 진입한다.
// ESC = 애플리케이션 종료(이 화면이 home).
class LauncherScreen final : public EditorScreen {
public:
    LauncherScreen();

    EditorScene Update(const platform::Input& in, float dt) override;
    void Render(platform::IRenderDevice& r) override;

private:
    ui::Menu m_menu;
};

} // namespace gs::app
