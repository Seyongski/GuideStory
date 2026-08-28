// 계정/채팅 서버(GuideStoryServer.exe)와 붙어있는 TCP 클라이언트.
//
// [왜 워커 스레드인가]
//   connect() / recv() 는 블로킹 호출이다. 게임 스레드(HostLoop)에서 부르면
//   서버가 꺼져 있을 때 창이 몇 초씩 멈춘다. 그래서 소켓 작업을 전부 별도 스레드로 뺀다.
//
// [스레드 경계 — 이 파일에서 가장 중요한 부분]
//
//     게임 스레드                          워커 스레드
//   ┌────────────────────┐              ┌───────────────────────────┐
//   │ LoginScreen        │              │  NetClient::Worker()      │
//   │ GameScreen         │              │                           │
//   │  RequestLogin()    │──송신 큐──▶  │   PumpSend()  -> socket   │
//   │  SendChat()        │              │                           │
//   │  Poll()            │◀─사건 큐──   │   PumpRecv()  <- socket   │
//   └────────────────────┘              └───────────────────────────┘
//
//   >> 워커 스레드는 IRenderDevice / Screen / Map 을 절대 건드리지 않는다. <<
//   순수 데이터(NetEvent)만 만들어 큐에 넣고, 게임 스레드가 Poll() 로 꺼내
//   그때 화면을 바꾼다. 이 규칙이 깨지면 재현이 어려운 랜덤 크래시가 난다.
//
// [소켓 타입이 여기 없는 이유]
//   winsock2.h 는 windows.h 보다 먼저 들어가야 해서(Socket.h 주석), SDL 을 쓰는
//   TU 에 섞이면 재정의 오류가 난다. 그래서 소켓은 .cpp 안 워커 함수의 지역 변수로만
//   존재하고 이 헤더에는 흔적이 없다. 화면 코드는 이 헤더만 포함하면 된다.
//
// 대응하는 서버 코드: GuideStoryServer/src/Server.cpp 의 ClientThread().
// 출처: TeamProject_MOU 의 FChatClientRunnable + UChatSubsystem 을 한 클래스로 합쳤다.
//   (언리얼 쪽은 UObject 수명 때문에 서브시스템과 러너블을 나눴지만, 여기서는
//    App 이 소유자 하나뿐이라 나눌 이유가 없다 — 나누면 파일만 늘어난다.)
#pragma once

#include "net/Protocol.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gs::net {

// 워커가 게임 스레드에 알리는 사건.
enum class NetEventType : uint8_t {
    Connecting,     // 접속 시도 시작
    Connected,      // TCP 연결됨 (아직 로그인 전)
    ConnectFailed,  // 접속 실패. text 에 사유
    Disconnected,   // 연결이 끊김 (워커는 자동으로 재시도한다)
    LoginAck,       // 로그인 응답. success / result / userId / name
    RegisterAck,    // 가입 응답. success / result
    Chat,           // 채팅 한 줄
};

// 사건 하나. 종류마다 쓰는 필드가 다르다(합집합 구조체 — 종류가 적어 variant 는 과하다).
struct NetEvent {
    NetEventType type      = NetEventType::Connecting;
    bool         success   = false;
    LoginResult  result    = LoginResult::Success;
    uint64_t     userId    = 0;                  // LoginAck: 내 계정 번호 / Chat: 보낸 사람
    ChatChannel  channel   = ChatChannel::All;   // Chat
    int64_t      timestamp = 0;                  // Chat (Unix epoch 초)
    std::string  name;                           // LoginAck: 서버가 확정한 내 닉네임 / Chat: 보낸 사람
    std::string  text;                           // Chat: 본문 / ConnectFailed: 사유
};

class NetClient {
public:
    NetClient() = default;
    ~NetClient();

    NetClient(const NetClient&)            = delete;
    NetClient& operator=(const NetClient&) = delete;

    // 워커 스레드를 띄운다. 연결은 워커가 알아서 하고, 끊기면 계속 재시도한다.
    // 서버가 꺼져 있어도 게임은 정상적으로 돌아간다(로그인만 안 될 뿐).
    void Start(std::string host, uint16_t port);

    // 워커에게 종료를 요청하고 합류한다. 소멸자가 자동으로 부르므로 보통 직접 부를 일은 없다.
    void Stop();

    // --- 게임 스레드에서 호출하는 요청 (조립만 하고 큐에 넣는다) ---
    bool RequestLogin(const std::string& loginId, const std::string& password);
    bool RequestRegister(const std::string& loginId, const std::string& password,
                         const std::string& nickname);
    bool SendChat(ChatChannel channel, const std::string& targetName, const std::string& text);

    // 로그아웃. 저장해 둔 자격을 지워 재연결 시 자동 로그인을 막고, 연결을 끊어
    // 서버가 이 세션을 정리하게 한다(워커는 곧바로 재접속하지만 미인증 상태다).
    void Logout();

    // 사건 하나를 꺼낸다. 게임 스레드 전용. 더 없으면 false.
    // 반환하기 전에 로그인 상태/닉네임 같은 게임 스레드 사본을 갱신한다.
    bool Poll(NetEvent& out);

    // --- 게임 스레드가 읽는 상태 ---
    bool Connected() const { return m_connected.load(std::memory_order_relaxed); }
    bool LoggedIn()  const { return m_loggedIn; }
    uint64_t           UserId()   const { return m_userId; }
    const std::string& Nickname() const { return m_nickname; }

    // 접속 대상. Start 이후로는 바뀌지 않으므로 락 없이 읽어도 된다.
    // 화면에 그대로 보여준다 — "연결 실패" 만 뜨고 어디로 붙으려 했는지 모르면 진단이 느리다.
    const std::string& Host() const { return m_host; }
    uint16_t           Port() const { return m_port; }

    // 아이디/비밀번호/닉네임 길이 규칙 검사(서버와 같은 규칙, Protocol.h 의 상수).
    // 클라가 미리 걸러 왕복을 아끼는 것일 뿐, 판정은 언제나 서버가 한다.
    static bool IsLoginIdValid(const std::string& s);
    static bool IsPasswordValid(const std::string& s);
    static bool IsNicknameValid(const std::string& s);

private:
    void Worker();                                  // 워커 스레드 본체(재연결 루프)
    void PushEvent(NetEvent&& ev);                  // 워커 -> 게임 스레드
    void Enqueue(std::vector<char>&& packet);       // 어느 쪽에서든 -> 송신 큐

    // --- 생성 후 불변(Start 에서 한 번 정한다) ---
    std::string m_host;
    uint16_t    m_port = 0;

    std::thread       m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_connected{false};   // 워커가 쓰고 게임 스레드가 읽는다

    mutable std::mutex            m_sendMutex;
    std::deque<std::vector<char>> m_sendQueue;   // 게임 스레드 -> 워커

    std::mutex           m_eventMutex;
    std::deque<NetEvent> m_events;               // 워커 -> 게임 스레드

    // [자동 재로그인]
    //   인게임 도중 연결이 잠깐 끊겼다 붙었을 때 사용자에게 다시 로그인시키면
    //   채팅이 조용히 죽는다. 그래서 마지막으로 성공한 자격을 들고 있다가 워커가 다시 보낸다.
    //   >> 비밀번호가 프로세스 메모리에 남는다는 뜻이다. 전송 구간도 평문이라
    //      어차피 같은 등급의 문제이며, TLS 도입 시 함께 정리한다 — tech-debt D-008. <<
    mutable std::mutex m_credMutex;
    std::string        m_credId;
    std::string        m_credPassword;
    bool               m_credValid = false;   // 서버가 한 번 성공을 돌려준 자격만 true

    // --- 게임 스레드 전용 사본 (Poll 이 갱신한다. 락 없이 읽어도 안전한 이유) ---
    bool        m_loggedIn = false;
    uint64_t    m_userId   = 0;
    std::string m_nickname;
};

} // namespace gs::net
