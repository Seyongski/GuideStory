#pragma once

#include "Screen.h"

#include "core/InputMap.h"
#include "core/KeySettingOverlay.h"
#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"

#include <memory>
#include <string>

namespace gs::app {

// 런타임 호스트 루프(합성 루트 다음 단계). 창/렌더러/타이밍을 소유하고
// 현재 장면(Screen)을 갱신·렌더하며, 장면이 요청한 전환을 수행한다.
// 시작 장면은 로그인창이다(로그인 → 메인화면 → 인게임).
// 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다 (ADR-006).
class App {
public:
    App(platform::IWindow& window, platform::IRenderDevice& renderer);

    // 종료(창 닫힘/ESC/게임종료) 전까지 입력 → 갱신 → 렌더를 반복한다.
    void Run();

private:
    std::unique_ptr<Screen> MakeScreen(SceneId id); // m_bindings를 GameScreen에 전달하므로 비정적

    // 키 바인딩 저장 파일 경로(assets/config/keybindings.txt).
    static std::string BindingsPath();

    platform::IWindow&       m_window;
    platform::IRenderDevice& m_renderer;
    core::InputMap           m_bindings;   // 키 바인딩(키세팅 대상) — 장면 전환과 무관하게 유지
    core::KeySettingOverlay  m_keySetting; // \ 키로 여는 키보드 설정 오버레이(m_bindings 편집)
    std::unique_ptr<Screen>  m_screen;
};

} // namespace gs::app
