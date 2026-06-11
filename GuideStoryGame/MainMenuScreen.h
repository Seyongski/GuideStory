#pragma once

#include "Screen.h"
#include "Ui.h"

namespace gs::app {

// 메인화면. 게임시작 / 환경설정 / 로그아웃 / 게임종료 버튼을 제공한다.
//  - 게임시작 : 인게임 진입(추후 캐릭터 선택창을 그 앞에 둘 예정 — 지금은 바로 진입).
//  - 환경설정 : 사운드 등 설정 UI 자리(1차 placeholder, 동작 없음).
//  - 로그아웃 : 로그인창으로 복귀.
//  - 게임종료 : 애플리케이션 종료.
class MainMenuScreen final : public Screen {
public:
    MainMenuScreen();

    SceneId Update(const platform::Input& in, float dt) override;
    void Render(platform::IRenderDevice& r) override;

private:
    ui::Menu m_menu;
};

} // namespace gs::app
