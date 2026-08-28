#pragma once

#include "Screen.h"

#include "core/Ui.h"

#include <string>

namespace gs::app {

// 로그인창. 서버(GuideStoryServer.exe)에 계정을 물어보고, 통과해야 메인화면으로 넘어간다.
//
// 한 화면에서 로그인과 가입을 모드로 전환한다 — 장면을 하나 더 만들면 접속 상태 표시와
// 서버 사건 처리(OnNetEvent)를 두 곳에 복사해야 하는데, 실제로 다른 것은 입력 칸 하나뿐이다.
//
// 조작: Tab = 다음 칸, Enter = 제출, 마우스 클릭 = 칸 포커스 / 버튼.
// (ESC는 App이 전역 종료로 처리한다.)
class LoginScreen final : public Screen {
public:
    explicit LoginScreen(net::NetClient& net);

    SceneId Update(const platform::Input& in, float dt) override;
    void Render(platform::IRenderDevice& r) override;
    void OnNetEvent(const net::NetEvent& ev) override;

private:
    enum class Mode { Login, Register };

    void SetMode(Mode mode);   // 버튼 라벨과 포커스를 모드에 맞게 다시 세운다
    void Submit();             // 현재 모드에 맞는 요청을 서버로 보낸다
    void FocusNext();          // Tab: 다음 입력 칸으로
    int  FieldCount() const { return m_mode == Mode::Register ? 3 : 2; }

    net::NetClient& m_net;     // App 소유 — 수명은 App이 보장

    ui::TextField m_id;
    ui::TextField m_password;
    ui::TextField m_nickname;  // 가입 모드에서만 쓴다
    ui::Toolbar   m_buttons;

    Mode        m_mode = Mode::Login;
    int         m_focus = 0;       // 0=아이디, 1=비밀번호, 2=닉네임
    bool        m_waiting = false; // 요청을 보내고 응답을 기다리는 중(중복 전송 방지)
    std::string m_message;         // 마지막 결과/안내 문구
    bool        m_messageIsError = false;
    SceneId     m_pending = SceneId::Stay; // OnNetEvent가 남긴 전환 요청(Update가 반환)
};

} // namespace gs::app
