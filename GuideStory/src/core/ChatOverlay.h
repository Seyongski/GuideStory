#pragma once

#include "core/Ui.h"
#include "math/Rect.h"
#include "platform/Color.h"
#include "platform/IRenderDevice.h"
#include "platform/Input.h"

#include <deque>
#include <string>
#include <vector>

// 인게임 채팅창(화면 좌하단). 메이플식으로 Enter 로 입력줄을 열고 Enter 로 보낸다.
//
// [이 클래스가 네트워크를 모르는 이유]
//   여기는 "무엇이 보이는가" 만 담당한다. 소켓·옵코드·채널 라우팅·재연결은 net::NetClient 가
//   하고, 둘을 잇는 것은 GameScreen 이다. 그래서 서버 없이도 이 위젯만 따로 띄워 볼 수 있다.
//   유일한 예외는 입력 한 줄의 바이트 상한(net::kMaxTextLen)이다 — 화면에서는 계속 쳐지는데
//   서버가 잘라 보내는 어긋남을 막으려면 그 값만은 프로토콜과 같아야 한다.
//
// [ESC 를 취소로 쓰지 않는 이유]
//   ESC 는 App 이 전역 종료로 먼저 가로챈다(App::Frame). 여기서 같은 키를 취소로 쓰면
//   "채팅 치다 ESC 눌렀더니 게임이 꺼지는" 사고가 난다. 그래서 빈 줄에서 Enter = 닫기다.
namespace gs::core {

class ChatOverlay {
public:
    // 화면 크기에 맞춰 패널/입력줄 위치를 잡는다. 화면 진입 시 한 번 부른다.
    void Layout(float screenW, float screenH);

    // 한 줄 추가. 색은 채널 의미(전체/귓속말/시스템)에 따라 호출측이 정한다.
    void Add(std::string text, const platform::Color& color);

    // 한 프레임 입력. 보낼 문장이 확정되면 outSubmitted 에 담고 true 를 반환한다.
    //   닫힘 상태 + Enter        -> 입력줄 열기
    //   열림 상태 + Enter(내용)  -> 전송(true) 후 입력줄 유지
    //   열림 상태 + Enter(빈 줄) -> 입력줄 닫기
    bool Update(const platform::Input& in, std::string& outSubmitted);

    // 입력줄이 열려 있는가. 열려 있으면 호출측(GameScreen)이 이동/점프 입력을 막아야 한다.
    // 안 그러면 "채팅을 치는 동안 캐릭터가 같이 움직인다".
    bool InputActive() const { return m_active; }

    // 줄바꿈 캐시를 채우므로 const 가 아니다(폭 계산에 렌더 디바이스의 폰트가 필요하다).
    void Render(platform::IRenderDevice& r);

private:
    struct Entry {
        std::string     text;
        platform::Color color;
    };

    // 패널 폭에 맞춰 긴 줄을 접는다. 새 줄이 들어오거나 배치가 바뀔 때만 다시 계산한다
    // (매 프레임 MeasureText 를 수백 번 부르지 않기 위한 캐시).
    void RebuildWrap(platform::IRenderDevice& r);

    std::deque<Entry>  m_lines;    // 오래된 것부터. 상한을 넘으면 앞에서 버린다
    std::vector<Entry> m_wrapped;  // 화면에 그릴 실제 줄(접힌 결과)
    bool               m_dirty = true;

    app::ui::TextField m_input;
    bool               m_active = false;   // 입력줄이 열려 있는가

    math::Rect m_panel{};   // 로그가 그려지는 영역
    math::Rect m_inputBox{};
};

} // namespace gs::core
