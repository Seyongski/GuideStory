#pragma once

#include "Screen.h"
#include "core/Ui.h"

#include <string>

namespace gs::app {

// 메인화면. 게임시작 / 환경설정 / 로그아웃 / 게임종료 버튼을 제공한다.
//  - 게임시작 : 인게임 진입(추후 캐릭터 선택창을 그 앞에 둘 예정 — 지금은 바로 진입).
//  - 환경설정 : 사운드 등 설정 UI 자리(1차 placeholder, 동작 없음).
//  - 로그아웃 : 로그인창으로 복귀. 자격 폐기(NetClient::Logout)는 App이 전환 시 수행한다.
//  - 게임종료 : 애플리케이션 종료.
class MainMenuScreen final : public Screen {
public:
    explicit MainMenuScreen(net::NetClient& net);

    SceneId Update(const platform::Input& in, float dt) override;
    void Render(platform::IRenderDevice& r) override;
    void OnNetEvent(const net::NetEvent& ev) override;

private:
    net::NetClient& m_net;    // App 소유 — 수명은 App이 보장
    ui::Menu        m_menu;

    // 연결이 끊겨 재로그인이 진행 중인 동안 표시할 안내. 비어 있으면 그리지 않는다.
    std::string m_notice;
};

} // namespace gs::app
