#pragma once

#include "net/NetClient.h"
#include "platform/IRenderDevice.h"
#include "platform/Input.h"

namespace gs::app {

// 1차: 윈도우 크기 고정(IWindow에 크기 질의 추가 전까지 상수). 메뉴 레이아웃의 기준.
inline constexpr float kViewW = 1366.0f;
inline constexpr float kViewH = 768.0f;

// 장면(화면) 식별자. Update가 반환해 App 호스트 루프가 전환을 수행한다.
//  - Stay     : 현재 장면 유지
//  - Login    : 로그인창(아이디/비밀번호/가입). 서버 인증을 통과해야 다음으로 간다.
//  - MainMenu : 메인화면(게임시작/환경설정/로그아웃/게임종료)
//  - InGame   : 인게임(플레이). 추후 캐릭터 선택창을 그 앞에 둘 자리.
//  - Quit     : 애플리케이션 종료
enum class SceneId { Stay, Login, MainMenu, InGame, Quit };

// 한 장면(로그인/메인메뉴/인게임)의 공통 인터페이스.
// 입력→갱신과 렌더만 책임지고, 창/타이밍/전환은 App 호스트 루프가 담당한다.
// SDL을 모르며 인터페이스(IRenderDevice/Input)에만 의존한다 (ADR-006).
class Screen {
public:
    virtual ~Screen() = default;

    // 이번 프레임 입력을 처리하고 전환 요청을 반환한다. SceneId::Stay면 현재 장면 유지.
    virtual SceneId Update(const platform::Input& in, float dt) = 0;

    // 백버퍼에 장면을 그린다(Clear 포함, Present는 App이 호출).
    virtual void Render(platform::IRenderDevice& r) = 0;

    // 서버에서 온 사건 하나. App이 매 프레임 큐를 비워 현재 장면에 전달한다(Update 직전).
    //
    // [왜 장면이 직접 Poll하지 않는가]
    //   아무도 꺼내지 않으면 워커 스레드가 넣은 사건이 무한정 쌓인다. 드레인 책임을
    //   App 한 곳에 두면 "어느 화면에 있든 큐는 비워진다"가 보장된다.
    //   전환이 필요하면 여기서 상태만 남기고 같은 프레임의 Update가 SceneId를 반환한다.
    virtual void OnNetEvent(const net::NetEvent& /*ev*/) {}
};

} // namespace gs::app
