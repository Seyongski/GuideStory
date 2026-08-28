// 접속한 클라이언트 하나의 상태와, 전체 세션 목록 관리.
//
// 고정 배열(`SOCKET clients[10]`)을 쓰지 않는 이유: 경계 검사가 없어 11번째 접속에서
// 배열 밖을 덮어쓰고, 소켓 핸들만 들고 있어서 "누가 보냈는지" 를 서버가 알 수 없다.
// 신원(userId/nickname)은 전부 서버가 채운다 — 클라이언트가 패킷에 담아 보낸 값을 쓰면
// 남을 사칭할 수 있다.
//
// 출처: Unreal-MOU/MOU_Server/Server/Session.h.
//   덜어낸 것: TeamId / bDead(팀·사망 채널) 와 친구 캐시. 전부 4인 리슨서버 co-op 전제의
//   개념이라 지속 월드(ADR-001)에는 맞지 않고, Protocol.h 에서도 이미 뺐다.
#pragma once

#include "net/Socket.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gs::server {

struct ClientSession {
    net::SocketHandle sock = net::kInvalidSocket;

    // --- 신원 정보 (로그인 성공 시 서버가 채운다) ---
    uint64_t    userId = 0;      // accounts.id
    std::string nickname;
    bool        authed = false;

    // accept() 시점에 읽은 상대 IP. 로그에만 쓴다.
    std::string peerAddress;

    // TCP 는 메시지 경계를 보장하지 않으므로 세션마다 누적 버퍼를 둔다.
    // 전역으로 두면 클라이언트끼리 데이터가 섞인다.
    std::vector<char> recvBuf;
};

using SessionPtr = std::shared_ptr<ClientSession>;

class SessionManager {
public:
    // 새 세션을 만들어 목록에 넣는다.
    SessionPtr Add(net::SocketHandle sock);

    // 목록에서 빼고 소켓을 닫는다. 둘 다 락 안에서 처리하므로
    // ForEach 순회 중에 소켓이 닫히는 일은 없다.
    void Remove(const SessionPtr& session);

    std::size_t Count();

    // 같은 계정이 이미 접속해 있는가(프로토콜의 AlreadyOnline 판정).
    // 이게 없으면 한 계정으로 여러 창을 띄워 자기 자신에게 귓속말하는 상태가 만들어진다.
    bool IsUserOnline(uint64_t userId);

    // 콜백을 세션 목록 락 안에서 실행한다.
    //
    // [트레이드오프] 브로드캐스트의 send() 가 이 락 안에서 일어난다. 수신자 하나가
    // 느리면 그동안 접속·종료가 막힌다. 접속자 수가 적은 지금은 단순함을 택했고,
    // 늘어나면 세션별 송신 큐(비동기 전송)로 바꿔야 한다 — tech-debt D-009.
    //
    // 주의: 콜백 안에서 Add / Remove 를 호출하면 데드락이 난다.
    template <typename Fn>
    void ForEach(Fn&& callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const SessionPtr& session : m_sessions) {
            callback(session);
        }
    }

private:
    std::mutex              m_mutex;
    std::vector<SessionPtr> m_sessions;
};

} // namespace gs::server
