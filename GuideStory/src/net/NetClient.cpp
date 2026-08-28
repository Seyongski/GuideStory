#include "net/NetClient.h"

// Socket.h / Framing.h 는 winsock2.h 를 끌고 온다. 이 .cpp 안에서만 쓰고
// NetClient.h 에는 새어나가지 않는다(헤더 주석 "소켓 타입이 여기 없는 이유").
#include "net/Framing.h"
#include "net/Socket.h"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace gs::net {

namespace {

// recv 대기 시간. 이 값이 곧 "종료 요청에 대한 반응 속도" 이자
// 송신 큐에 넣은 패킷이 실제로 나가기까지의 최대 지연이다.
constexpr int kRecvTimeoutMs = 50;

// 접속 실패 후 재시도까지의 간격. 서버가 꺼져 있어도 로그가 폭주하지 않게 한다.
constexpr int kReconnectDelayMs = 3000;

// connect() 상한. 이게 없으면 죽은 IP 로 접속을 시도할 때 OS 기본값(수십 초)까지
// 워커가 붙잡혀서, 그동안 창을 닫아도 프로세스가 남는다.
constexpr int kConnectTimeoutMs = 3000;

// 하트비트 주기. 오래 조용한 TCP 연결을 공유기/방화벽이 끊는 것을 막는다.
constexpr int kHeartbeatSeconds = 20;

// 종료 요청에 빨리 반응하도록 잘게 쪼개 잔다.
void SleepSliced(int totalMs, const std::atomic<bool>& stop) {
    for (int slept = 0; slept < totalMs && !stop.load(std::memory_order_relaxed);
         slept += kRecvTimeoutMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kRecvTimeoutMs));
    }
}

// 이름 해석 + 논블로킹 connect + select 로 상한을 건다.
// 성공하면 블로킹 모드로 되돌린 소켓을, 실패하면 kInvalidSocket 을 돌려준다.
SocketHandle ConnectWithTimeout(const std::string& host, uint16_t port, std::string& outError) {
    char portText[8] = {};
    std::snprintf(portText, sizeof(portText), "%u", static_cast<unsigned>(port));

    addrinfo hints{};
    hints.ai_family   = AF_INET;      // IPv4 만. 서버도 AF_INET 으로 bind 한다
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* list = nullptr;
    // DNS 조회는 블로킹이다. 여기가 워커 스레드라 게임 스레드는 영향받지 않는다.
    if (::getaddrinfo(host.c_str(), portText, &hints, &list) != 0 || list == nullptr) {
        outError = "주소를 찾을 수 없습니다: " + host;
        return kInvalidSocket;
    }

    SocketHandle sock = ::socket(list->ai_family, list->ai_socktype, list->ai_protocol);
    if (sock == kInvalidSocket) {
        ::freeaddrinfo(list);
        outError = "소켓 생성 실패";
        return kInvalidSocket;
    }

    SetNonBlocking(sock, true);
    const int rc = ::connect(sock, list->ai_addr, static_cast<int>(list->ai_addrlen));
    ::freeaddrinfo(list);

    if (rc != 0) {
        if (!IsConnectPending(LastNetError())) {
            CloseSocket(sock);
            outError = "서버에 연결할 수 없습니다";
            return kInvalidSocket;
        }

        // 진행 중이다. 쓰기 가능해지면 연결 절차가 끝난 것이다.
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(sock, &writable);
        timeval timeout{};
        timeout.tv_sec  = kConnectTimeoutMs / 1000;
        timeout.tv_usec = (kConnectTimeoutMs % 1000) * 1000;

        if (::select(static_cast<int>(sock) + 1, nullptr, &writable, nullptr, &timeout) <= 0) {
            CloseSocket(sock);
            outError = "서버 응답이 없습니다 (연결 시간 초과)";
            return kInvalidSocket;
        }

        // select 가 깨어난 것과 연결 성공은 다르다. 거부(RST)도 쓰기 가능으로 잡히므로
        // SO_ERROR 를 반드시 확인해야 한다 — 이 검사가 없으면 죽은 소켓으로 계속 send 한다.
        int soError = 0;
        socklen_t len = static_cast<socklen_t>(sizeof(soError));
        if (::getsockopt(sock, SOL_SOCKET, SO_ERROR,
                         reinterpret_cast<char*>(&soError), &len) != 0 || soError != 0) {
            CloseSocket(sock);
            outError = "서버가 연결을 거부했습니다";
            return kInvalidSocket;
        }
    }

    SetNonBlocking(sock, false);          // 이후로는 타임아웃이 걸린 블로킹 recv 를 쓴다
    SetRecvTimeout(sock, kRecvTimeoutMs);
    return sock;
}

} // namespace

NetClient::~NetClient() {
    Stop();
}

void NetClient::Start(std::string host, uint16_t port) {
    if (m_thread.joinable()) return;   // 이미 돌고 있다

    m_host = std::move(host);
    m_port = port;
    m_stop.store(false, std::memory_order_relaxed);
    m_thread = std::thread(&NetClient::Worker, this);
}

void NetClient::Stop() {
    if (!m_thread.joinable()) return;
    m_stop.store(true, std::memory_order_relaxed);
    m_thread.join();   // 워커는 최대 kRecvTimeoutMs 안에 플래그를 본다
}

bool NetClient::IsLoginIdValid(const std::string& s) {
    return s.size() >= kMinLoginIdLen && s.size() < kMaxLoginIdLen;
}
bool NetClient::IsPasswordValid(const std::string& s) {
    return s.size() >= kMinPasswordLen && s.size() < kMaxPasswordLen;
}
bool NetClient::IsNicknameValid(const std::string& s) {
    return s.size() >= kMinNicknameLen && s.size() < kMaxNameLen;
}

void NetClient::Enqueue(std::vector<char>&& packet) {
    if (packet.empty()) return;
    std::lock_guard<std::mutex> lock(m_sendMutex);
    m_sendQueue.push_back(std::move(packet));
}

void NetClient::PushEvent(NetEvent&& ev) {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.push_back(std::move(ev));
}


bool NetClient::RequestLogin(const std::string& loginId, const std::string& password) {
    if (!IsLoginIdValid(loginId) || !IsPasswordValid(password)) return false;

    // 아직 "성공한 자격" 은 아니다. 서버가 성공을 돌려줄 때 m_credValid 가 선다.
    // 틀린 비밀번호를 저장해두면 재연결마다 실패 로그인을 반복하게 된다.
    {
        std::lock_guard<std::mutex> lock(m_credMutex);
        m_credId       = loginId;
        m_credPassword = password;
        m_credValid    = false;
    }

    LoginReqBody body{};
    body.Version = kProtocolVersion;
    CopyFixedString(body.LoginId,  kMaxLoginIdLen,  loginId);
    CopyFixedString(body.Password, kMaxPasswordLen, password);
    Enqueue(BuildPacket(Opcode::LoginReq, &body, sizeof(body)));
    return true;
}

bool NetClient::RequestRegister(const std::string& loginId, const std::string& password,
                                const std::string& nickname) {
    if (!IsLoginIdValid(loginId) || !IsPasswordValid(password) || !IsNicknameValid(nickname))
        return false;

    RegisterReqBody body{};
    body.Version = kProtocolVersion;
    CopyFixedString(body.LoginId,  kMaxLoginIdLen,  loginId);
    CopyFixedString(body.Password, kMaxPasswordLen, password);
    CopyFixedString(body.Nickname, kMaxNameLen,     nickname);
    Enqueue(BuildPacket(Opcode::RegisterReq, &body, sizeof(body)));
    return true;
}

bool NetClient::SendChat(ChatChannel channel, const std::string& targetName,
                         const std::string& text) {
    if (text.empty() || text.size() > kMaxTextLen) return false;
    if (channel == ChatChannel::System) return false;   // 시스템 메시지는 서버만 만든다

    ChatSendBody body{};
    body.Channel = static_cast<uint8_t>(channel);
    body.TextLen = static_cast<uint16_t>(text.size());
    CopyFixedString(body.TargetName, kMaxNameLen, targetName);

    Enqueue(BuildPacket(Opcode::ChatSend, &body, sizeof(body),
                        text.data(), static_cast<uint32_t>(text.size())));
    return true;
}

void NetClient::Logout() {
    {
        std::lock_guard<std::mutex> lock(m_credMutex);
        m_credId.clear();
        m_credPassword.clear();
        m_credValid = false;
    }
    // 큐에 남은 패킷은 이전 계정의 것이다. 다음 로그인에 섞이면 안 된다.
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        m_sendQueue.clear();
    }
    m_loggedIn = false;
    m_userId   = 0;
    m_nickname.clear();

    // 연결 자체는 끊지 않는다 — 서버는 같은 소켓에서 재로그인을 허용하고,
    // 끊었다 붙이면 재접속 대기(3초) 동안 로그인 화면이 그만큼 먹통이 된다.
}

bool NetClient::Poll(NetEvent& out) {
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        if (m_events.empty()) return false;
        out = std::move(m_events.front());
        m_events.pop_front();
    }

    // 게임 스레드 사본 갱신. 여기서만 쓰므로 화면 코드는 락 없이 읽어도 된다.
    switch (out.type) {
        case NetEventType::LoginAck:
            if (out.success) {
                m_loggedIn = true;
                m_userId   = out.userId;
                m_nickname = out.name;
            }
            break;
        case NetEventType::Disconnected:
        case NetEventType::ConnectFailed:
            m_loggedIn = false;   // 재연결되면 워커가 자동으로 다시 로그인한다
            break;
        default:
            break;
    }
    return true;
}

// ----------------------------------------------------------------------------
// 워커 스레드
// ----------------------------------------------------------------------------
void NetClient::Worker() {
    if (!NetInit()) {
        NetEvent ev;
        ev.type = NetEventType::ConnectFailed;
        ev.text = "네트워크 초기화 실패 (WSAStartup)";
        PushEvent(std::move(ev));
        return;
    }

    std::vector<char> recvBuf;      // 서버의 ClientSession::recvBuf 와 같은 역할
    std::vector<char> body;         // TryExtractPacket 이 바디를 담는 임시 버퍼
    char temp[1024];

    // 바깥 루프 = 재연결 루프. 서버가 꺼져 있거나 도중에 죽어도 게임은 계속 돌아가야 하므로
    // 실패를 치명적 오류로 보지 않고 계속 재시도한다.
    while (!m_stop.load(std::memory_order_relaxed)) {
        {
            NetEvent ev;
            ev.type = NetEventType::Connecting;
            PushEvent(std::move(ev));
        }

        std::string error;
        const SocketHandle sock = ConnectWithTimeout(m_host, m_port, error);
        if (sock == kInvalidSocket) {
            NetEvent ev;
            ev.type = NetEventType::ConnectFailed;
            ev.text = std::move(error);
            PushEvent(std::move(ev));
            SleepSliced(kReconnectDelayMs, m_stop);
            continue;
        }

        // 이전 연결에서 남은 조각이 새 연결에 섞이면 프레이밍이 깨진다.
        recvBuf.clear();

        // 송신 큐도 비운다. 새 연결에서는 LoginReq 가 가장 먼저 나가야 하는데,
        // 끊기기 직전 큐에 남아 있던 채팅이 먼저 나가면 서버가 미인증으로 보고 버린다.
        {
            std::lock_guard<std::mutex> lock(m_sendMutex);
            m_sendQueue.clear();
        }

        // 자동 재로그인: 한 번 성공한 자격이 있으면 사용자를 다시 귀찮게 하지 않는다.
        {
            std::lock_guard<std::mutex> credLock(m_credMutex);
            if (m_credValid) {
                LoginReqBody req{};
                req.Version = kProtocolVersion;
                CopyFixedString(req.LoginId,  kMaxLoginIdLen,  m_credId);
                CopyFixedString(req.Password, kMaxPasswordLen, m_credPassword);
                std::lock_guard<std::mutex> sendLock(m_sendMutex);
                m_sendQueue.push_back(BuildPacket(Opcode::LoginReq, &req, sizeof(req)));
            }
        }

        m_connected.store(true, std::memory_order_relaxed);
        {
            NetEvent ev;
            ev.type = NetEventType::Connected;
            PushEvent(std::move(ev));
        }

        auto lastBeat = std::chrono::steady_clock::now();
        bool alive = true;

        // 안쪽 루프 = 연결이 살아있는 동안의 송수신.
        while (alive && !m_stop.load(std::memory_order_relaxed)) {
            // 1) 송신 큐 비우기. 락을 잡은 채로 send 하면 게임 스레드가 그동안 막히므로
            //    큐를 통째로 꺼낸 뒤 락 밖에서 보낸다.
            std::deque<std::vector<char>> pending;
            {
                std::lock_guard<std::mutex> lock(m_sendMutex);
                pending.swap(m_sendQueue);
            }
            for (const std::vector<char>& packet : pending) {
                if (!SendAll(sock, packet.data(), static_cast<int32_t>(packet.size()))) {
                    alive = false;
                    break;
                }
            }
            if (!alive) break;

            // 2) 하트비트. 오래 조용하면 중간 장비가 연결을 끊는다.
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastBeat).count()
                >= kHeartbeatSeconds) {
                lastBeat = now;
                if (!SendPacket(sock, Opcode::Heartbeat, nullptr, 0)) break;
            }

            // 3) 수신. 타임아웃이 걸려 있어 최대 kRecvTimeoutMs 안에 돌아온다.
            const int received = ::recv(sock, temp, sizeof(temp), 0);
            if (received == 0) break;                        // 서버가 정상 종료
            if (received < 0) {
                if (IsRecvTimeout(LastNetError())) continue; // 할 말이 없었을 뿐
                break;                                       // 진짜 에러
            }

            recvBuf.insert(recvBuf.end(), temp, temp + received);

            // 한 번의 recv 에 여러 패킷이 붙어 왔을 수 있으므로 다 꺼낼 때까지 돈다.
            PacketHeader header{};
            for (;;) {
                const FrameResult result = TryExtractPacket(recvBuf, header, body);
                if (result == FrameResult::NeedMore) break;
                if (result == FrameResult::Malformed) {
                    std::fprintf(stderr, "[net] 비정상 패킷 크기. 연결을 끊는다.\n");
                    alive = false;
                    break;
                }

                const char* data = body.empty() ? nullptr : body.data();
                const uint32_t size = static_cast<uint32_t>(body.size());

                switch (static_cast<Opcode>(header.Opcode)) {
                    case Opcode::LoginAck: {
                        if (size < sizeof(LoginAckBody)) break;
                        LoginAckBody ack{};
                        std::memcpy(&ack, data, sizeof(ack));

                        NetEvent ev;
                        ev.type    = NetEventType::LoginAck;
                        ev.success = (ack.bSuccess != 0);
                        ev.result  = static_cast<LoginResult>(ack.Result);
                        ev.userId  = ack.UserId;
                        ev.name    = ReadFixedString(ack.Nickname, kMaxNameLen);

                        // 성공한 자격만 저장한다 — 재연결 때 이 값으로 자동 로그인한다.
                        {
                            std::lock_guard<std::mutex> lock(m_credMutex);
                            m_credValid = ev.success && !m_credId.empty();
                        }
                        PushEvent(std::move(ev));
                        break;
                    }
                    case Opcode::RegisterAck: {
                        if (size < sizeof(RegisterAckBody)) break;
                        RegisterAckBody ack{};
                        std::memcpy(&ack, data, sizeof(ack));

                        NetEvent ev;
                        ev.type    = NetEventType::RegisterAck;
                        ev.success = (ack.bSuccess != 0);
                        ev.result  = static_cast<LoginResult>(ack.Result);
                        PushEvent(std::move(ev));
                        break;
                    }
                    case Opcode::ChatBroadcast: {
                        if (size < sizeof(ChatBroadcastBody)) break;
                        ChatBroadcastBody msg{};
                        std::memcpy(&msg, data, sizeof(msg));

                        // 선언한 길이와 실제 도착한 바이트가 맞는지 확인한다.
                        // 이 검사가 없으면 위조된 TextLen 으로 버퍼 밖을 읽게 된다.
                        if (msg.TextLen > kMaxTextLen) break;
                        if (sizeof(ChatBroadcastBody) + msg.TextLen != size) break;

                        NetEvent ev;
                        ev.type      = NetEventType::Chat;
                        ev.userId    = msg.SenderUserId;
                        ev.channel   = static_cast<ChatChannel>(msg.Channel);
                        ev.timestamp = msg.Timestamp;
                        ev.name      = ReadFixedString(msg.SenderName, kMaxNameLen);
                        ev.text.assign(data + sizeof(ChatBroadcastBody), msg.TextLen);
                        PushEvent(std::move(ev));
                        break;
                    }
                    case Opcode::Heartbeat:
                        break;   // 서버가 살아있다는 신호. 따로 할 일은 없다
                    default:
                        std::fprintf(stderr, "[net] 알 수 없는 오피코드 %u\n", header.Opcode);
                        break;
                }
            }
        }

        m_connected.store(false, std::memory_order_relaxed);
        ShutdownSend(sock);
        CloseSocket(sock);
        {
            NetEvent ev;
            ev.type = NetEventType::Disconnected;
            PushEvent(std::move(ev));
        }

        if (!m_stop.load(std::memory_order_relaxed)) {
            SleepSliced(kReconnectDelayMs, m_stop);
        }
    }

    NetShutdown();
}

} // namespace gs::net
