#pragma once

#include "Screen.h"

#include "core/HostLoop.h"
#include "core/InputMap.h"
#include "core/KeySettingOverlay.h"
#include "core/PlayerState.h"
#include "net/NetClient.h"
#include "net/ServerConfig.h"
#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"

#include <memory>
#include <string>

namespace gs::app {

// 런타임 호스트 루프. 타이밍·폴·Present 골격은 core::HostLoop(Template Method)이 맡고,
// 이 클래스는 한 프레임의 의미(현재 장면 갱신·렌더, 전환, 키세팅 오버레이, 전역 ESC,
// 서버 사건 배달)만 Frame()으로 채운다.
// 시작 장면은 로그인창이다(로그인 → 메인화면 → 인게임).
// 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다 (ADR-006).
class App : public core::HostLoop {
public:
    App(platform::IWindow& window, platform::IRenderDevice& renderer);

protected:
    // 한 프레임: 서버 사건 배달, 오버레이 토글/갱신, 전역 ESC, 장면 갱신·전환·렌더. false면 앱 종료.
    bool Frame(const platform::Input& in, float dt) override;

private:
    std::unique_ptr<Screen> MakeScreen(SceneId id); // m_bindings/m_net을 화면에 전달하므로 비정적

    // 키 바인딩 저장 파일 경로(assets/config/keybindings.txt).
    static std::string BindingsPath();
    // 플레이어 상태(마지막 맵) 저장 파일 경로(assets/config/playerstate.txt).
    static std::string PlayerStatePath();
    // 접속할 서버 주소 파일 경로(assets/config/server.txt). 없으면 기본값(127.0.0.1:7777).
    static std::string ServerConfigPath();

    core::InputMap           m_bindings;   // 키 바인딩(키세팅 대상) — 장면 전환과 무관하게 유지
    core::KeySettingOverlay  m_keySetting; // \ 키로 여는 키보드 설정 오버레이(m_bindings 편집)
    core::PlayerState        m_playerState; // 마지막 맵 기억 — GameScreen이 진입/전환 시 갱신·저장

    // 네트워크는 앱 수명 내내 하나만 두고 장면들이 참조로 공유한다.
    // 장면마다 새로 연결하면 화면을 옮길 때마다 접속이 끊겨 채팅이 죽는다.
    // ★ m_screen보다 먼저 선언해야 한다 — 화면들이 이 참조를 들고 있으므로 나중에 소멸해야 한다.
    net::ServerConfig        m_serverCfg;
    net::NetClient           m_net;

    std::unique_ptr<Screen>  m_screen;
};

} // namespace gs::app
