#pragma once

#include "Screen.h"

#include "core/HostLoop.h"
#include "core/InputMap.h"
#include "core/KeySettingOverlay.h"
#include "core/PlayerState.h"
#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"

#include <memory>
#include <string>

namespace gs::app {

// 런타임 호스트 루프. 타이밍·폴·Present 골격은 core::HostLoop(Template Method)이 맡고,
// 이 클래스는 한 프레임의 의미(현재 장면 갱신·렌더, 전환, 키세팅 오버레이, 전역 ESC)만 Frame()으로 채운다.
// 시작 장면은 로그인창이다(로그인 → 메인화면 → 인게임).
// 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다 (ADR-006).
class App : public core::HostLoop {
public:
    App(platform::IWindow& window, platform::IRenderDevice& renderer);

protected:
    // 한 프레임: 오버레이 토글/갱신, 전역 ESC, 장면 갱신·전환·렌더. false면 앱 종료.
    bool Frame(const platform::Input& in, float dt) override;

private:
    std::unique_ptr<Screen> MakeScreen(SceneId id); // m_bindings를 GameScreen에 전달하므로 비정적

    // 키 바인딩 저장 파일 경로(assets/config/keybindings.txt).
    static std::string BindingsPath();
    // 플레이어 상태(마지막 맵) 저장 파일 경로(assets/config/playerstate.txt).
    static std::string PlayerStatePath();

    core::InputMap           m_bindings;   // 키 바인딩(키세팅 대상) — 장면 전환과 무관하게 유지
    core::KeySettingOverlay  m_keySetting; // \ 키로 여는 키보드 설정 오버레이(m_bindings 편집)
    core::PlayerState        m_playerState; // 마지막 맵 기억 — GameScreen이 진입/전환 시 갱신·저장
    std::unique_ptr<Screen>  m_screen;
};

} // namespace gs::app
