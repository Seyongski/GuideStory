// GuideStory 계정/채팅 서버.
//
// 클라이언트(GuideStoryGame.exe)와 **같은 프로토콜 헤더**(GuideStory/src/net/Protocol.h)를
// 컴파일한다. 한쪽만 고치면 조용히 어긋나므로 헤더를 공유해서 그럴 여지를 없앴다.
//
// [지금 이 서버가 하는 일]
//   가입 / 로그인(계정 = SQLite + PBKDF2) / 채팅(전체·귓속말·시스템) / 하트비트.
//   월드 상태(누가 어느 맵에 있는가)는 아직 모른다 — 그것이 ADR-001 데디케이트 서버의
//   다음 단계이고, 이 단계는 "인증된 세션과 그들 사이의 메시지" 까지를 맡는다.
//
// [모델: 접속자 1명당 스레드 1개 (thread-per-connection)]
//   구현이 단순하고 흐름을 그대로 읽을 수 있어 먼저 이것으로 세운다.
//   접속자가 늘면 스레드 수가 그대로 늘어 컨텍스트 스위칭이 비용이 된다 —
//   IOCP(윈도우) / epoll(리눅스) 비교가 ADR-001 의 증명 과제다. 그 비교의 기준선이 이 코드다.
//
// 사용법: GuideStoryServer.exe [port] [db경로]      (기본 7777 / guidestory.db)
//
// 출처: Unreal-MOU/MOU_Server/Server/Server.cpp.
//   가져온 것: 프레이밍 루프, 세션 관리, 로그인/가입 핸들러, 채널 라우팅, Ctrl+C 정리.
//   덜어낸 것: 방/로비, 팀·사망 채널, 친구·메신저 — 전부 4인 co-op 전제의 기능이라
//              지속 월드에는 맞지 않는다. 안 쓰는 옵코드를 옮겨두면 죽은 프로토콜만 남는다.
//   더한 것  : 귓속말 라우팅(MOU 는 미구현이었다), 중복 접속 거부, 접속/퇴장 시스템 알림.

#include "Accounts.h"
#include "ChatLog.h"
#include "Session.h"

#include "net/Framing.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>
#include <vector>

using namespace gs;
using namespace gs::server;
using net::Opcode;
using net::ChatChannel;
using net::LoginResult;

namespace {

SessionManager    gSessions;
std::atomic<bool> gRunning{true};

// Ctrl+C 로 서버를 내릴 때 큐에 남은 채팅 로그를 마저 쓰고 나간다.
// 이게 없으면 accept() 에서 블록된 채 프로세스가 즉사해서
// 아직 커밋 안 된 로그가 통째로 사라진다.
//
// 윈도우 CRT 는 SIGINT 핸들러를 별도 스레드에서 호출하므로
// 여기서 chatlog::Stop() 이 라이터 스레드를 join 해도 데드락이 나지 않는다.
void OnInterrupt(int) {
    gRunning = false;
    chatlog::Stop();
    accounts::Stop();
    std::_Exit(0);   // 소켓과 메모리 회수는 OS 에 맡긴다
}

// 닉네임 비교. DB 가 COLLATE NOCASE 라 대소문자를 구분하지 않으므로 여기서도 맞춘다.
// ASCII 만 접는다 — 한글에는 대소문자가 없어 이것으로 충분하다.
bool EqualsIgnoreAsciiCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 전송
//
// [규칙 — 이 파일에서 가장 중요한 불변식]
//   **모든 send 는 세션 목록 락 안에서 한다(= ForEach 콜백 안).**
//   SendAll 은 부분 전송을 루프로 처리하므로 원자적이지 않다. 두 스레드가 같은 소켓에
//   동시에 쓰면 한 패킷의 바이트 사이에 다른 패킷이 끼어들어 수신측 프레이밍이 깨진다.
//   락이 그 직렬화를 대신한다. (대가는 Session.h 의 ForEach 주석 — tech-debt D-009.)
//
//   그래서 아래 함수들은 절대 ForEach 안에서 부르면 안 된다(재귀 락 = 데드락).
// ---------------------------------------------------------------------------

// 특정 세션 하나에게 보낸다. 대상을 목록에서 찾는 동안 락이 걸려 있으므로,
// 그 사이에 상대가 나가서 소켓이 닫히는 경합이 없다.
void SendTo(const SessionPtr& target, Opcode op,
            const void* head, uint32_t headSize,
            const void* tail = nullptr, uint32_t tailSize = 0) {
    gSessions.ForEach([&](const SessionPtr& s) {
        if (s == target && s->sock != net::kInvalidSocket) {
            net::SendPacket2(s->sock, op, head, headSize, tail, tailSize);
        }
    });
}

// 서버가 만든 시스템 메시지. 클라이언트가 System 채널로 보내오면 거부하므로(RouteChat),
// 이 채널의 문장은 언제나 서버가 쓴 것이다.
net::ChatBroadcastBody MakeSystemHead(uint16_t textLen) {
    net::ChatBroadcastBody head{};
    head.SenderUserId = 0;                                    // 사람이 아님
    head.Timestamp    = static_cast<int64_t>(std::time(nullptr));
    head.TextLen      = textLen;
    head.Channel      = static_cast<uint8_t>(ChatChannel::System);
    net::CopyFixedString(head.SenderName, net::kMaxNameLen, "");
    return head;
}

void SendSystemTo(const SessionPtr& target, const std::string& text) {
    const net::ChatBroadcastBody head = MakeSystemHead(static_cast<uint16_t>(text.size()));
    SendTo(target, Opcode::ChatBroadcast, &head, sizeof(head),
           text.data(), static_cast<uint32_t>(text.size()));
}

// 인증된 전원에게. except 가 있으면 그 한 명은 건너뛴다(본인에게는 다른 문장을 보낼 때).
void BroadcastSystem(const std::string& text, const SessionPtr& except = nullptr) {
    const net::ChatBroadcastBody head = MakeSystemHead(static_cast<uint16_t>(text.size()));
    gSessions.ForEach([&](const SessionPtr& s) {
        if (!s->authed || s == except) return;
        net::SendPacket2(s->sock, Opcode::ChatBroadcast, &head, sizeof(head),
                         text.data(), static_cast<uint32_t>(text.size()));
    });
    std::printf("[시스템] %s\n", text.c_str());
}

// ---------------------------------------------------------------------------
// 채팅 라우팅
//
// "무조건 전원에게" 가 아니라 채널에 따라 수신자를 고른다.
// 맵/파티 채널은 서버가 월드 상태를 갖는 시점에 여기 case 하나로 늘어난다.
// ---------------------------------------------------------------------------
void RouteChat(const SessionPtr& sender, ChatChannel channel,
               const std::string& targetName, const char* text, uint16_t textLen) {
    // 발화 자격 검증. 클라이언트가 보낸 채널 값을 그대로 믿지 않는다.
    if (channel == ChatChannel::System) {
        std::printf("[거부] %s 가 시스템 채널로 발화를 시도했다\n", sender->nickname.c_str());
        return;
    }

    if (channel == ChatChannel::Whisper &&
        EqualsIgnoreAsciiCase(sender->nickname, targetName)) {
        // 아래 루프는 자기 자신을 건너뛰므로 "찾을 수 없습니다" 가 나가버린다.
        // 사용자가 오타를 의심하게 되므로 실제 사유를 따로 알려준다.
        SendSystemTo(sender, "자기 자신에게는 귓속말할 수 없습니다.");
        return;
    }

    net::ChatBroadcastBody head{};
    head.SenderUserId = sender->userId;                        // 서버 보관값
    head.Timestamp    = static_cast<int64_t>(std::time(nullptr));
    head.TextLen      = textLen;
    head.Channel      = static_cast<uint8_t>(channel);
    // 보낸 사람 이름도 서버가 채운다. 클라가 보낸 값을 옮기면 남을 사칭할 수 있다.
    net::CopyFixedString(head.SenderName, net::kMaxNameLen, sender->nickname);

    int delivered = 0;
    gSessions.ForEach([&](const SessionPtr& target) {
        if (!target->authed) return;

        bool deliver = false;
        switch (channel) {
            case ChatChannel::All:
                deliver = true;
                break;
            case ChatChannel::Whisper:
                // 자기 자신은 제외한다. 본인에게는 아래에서 "누구에게 보냈다" 를 따로 알린다.
                deliver = (target != sender) && EqualsIgnoreAsciiCase(target->nickname, targetName);
                break;
            default:
                break;
        }

        if (deliver) {
            net::SendPacket2(target->sock, Opcode::ChatBroadcast, &head, sizeof(head),
                             text, textLen);
            ++delivered;
        }
    });

    if (channel == ChatChannel::Whisper) {
        // 귓속말은 보낸 사람도 결과를 알아야 한다. 아무 표시가 없으면
        // "보냈는데 상대가 씹은 것" 과 "그런 사람이 없어서 안 간 것" 을 구분할 수 없다.
        if (delivered == 0) {
            SendSystemTo(sender, "'" + targetName + "' 님을 찾을 수 없습니다.");
        } else {
            SendSystemTo(sender, "[" + targetName + " 님에게] " + std::string(text, textLen));
        }
    }

    std::printf("[%s] %s: %.*s  (수신 %d명)\n",
                net::ChannelName(channel), sender->nickname.c_str(),
                static_cast<int>(textLen), text, delivered);

    // 수신자가 0명이어도 기록은 남긴다 — "아무도 못 들었지만 말한 것은 사실" 이다.
    // Enqueue 는 큐에 넣기만 하고 바로 돌아오므로 여기서 디스크를 기다리지 않는다.
    chatlog::Enqueue(head.Timestamp, sender->userId, sender->nickname,
                     static_cast<uint8_t>(channel), targetName, text, textLen);
}

// ---------------------------------------------------------------------------
// 패킷 핸들러. false 를 반환하면 연결을 끊는다.
// ---------------------------------------------------------------------------

// 계정 모듈의 결과를 프로토콜 사유 코드로 옮긴다.
// 두 enum 을 따로 두는 이유는 계정 모듈이 프로토콜을 몰라도 되게 하기 위함이다.
LoginResult ToLoginResult(AccountResult r) {
    switch (r) {
        case AccountResult::Success:       return LoginResult::Success;
        case AccountResult::NotFound:      return LoginResult::AccountNotFound;
        case AccountResult::WrongPassword: return LoginResult::WrongPassword;
        case AccountResult::DuplicateId:   return LoginResult::DuplicateId;
        case AccountResult::DuplicateNick: return LoginResult::DuplicateNick;
        case AccountResult::InvalidFormat: return LoginResult::InvalidFormat;
        default:                           return LoginResult::ServerError;
    }
}

void SendLoginFailure(const SessionPtr& session, LoginResult reason) {
    // 그냥 연결을 끊어버리면 클라이언트는 원인을 모른 채 재접속만 반복한다.
    net::LoginAckBody ack{};
    ack.bSuccess      = 0;
    ack.Result        = static_cast<uint8_t>(reason);
    ack.ServerVersion = net::kProtocolVersion;
    SendTo(session, Opcode::LoginAck, &ack, sizeof(ack));
}

bool HandleLoginReq(const SessionPtr& session, const char* body, uint32_t bodySize) {
    // Version 은 LoginReqBody 의 첫 필드다.
    // 구조체 크기가 안 맞더라도 이 2바이트만은 읽어서 정확한 사유를 돌려준다.
    if (bodySize < sizeof(uint16_t)) {
        std::printf("[거부] LoginReq 가 너무 짧다 (%u바이트)\n", bodySize);
        SendLoginFailure(session, LoginResult::InvalidRequest);
        return false;
    }

    uint16_t clientVersion = 0;
    std::memcpy(&clientVersion, body, sizeof(clientVersion));
    if (clientVersion != net::kProtocolVersion) {
        std::printf("[거부] 프로토콜 버전 불일치. 클라이언트=%u, 서버=%u"
                    " (양쪽을 같은 커밋으로 다시 빌드할 것)\n",
                    clientVersion, net::kProtocolVersion);
        SendLoginFailure(session, LoginResult::VersionMismatch);
        return false;
    }

    if (bodySize < sizeof(net::LoginReqBody)) {
        std::printf("[거부] LoginReq 크기 부족 (%u < %u)\n",
                    bodySize, static_cast<uint32_t>(sizeof(net::LoginReqBody)));
        SendLoginFailure(session, LoginResult::InvalidRequest);
        return false;
    }

    net::LoginReqBody req{};
    std::memcpy(&req, body, sizeof(req));

    const std::string loginId  = net::ReadFixedString(req.LoginId,  net::kMaxLoginIdLen);
    const std::string password = net::ReadFixedString(req.Password, net::kMaxPasswordLen);

    // 같은 소켓에서 로그아웃 후 다시 로그인하는 경우. 먼저 이전 신원을 정리해야
    // 아래 중복 접속 검사가 자기 자신을 보고 AlreadyOnline 을 내는 일이 없다.
    if (session->authed) {
        session->authed = false;
        BroadcastSystem(session->nickname + " 님이 나갔습니다.");
    }

    uint64_t    accountId = 0;
    std::string nickname;
    const AccountResult authResult =
        accounts::Authenticate(loginId, password, accountId, nickname);

    if (authResult != AccountResult::Success) {
        std::printf("[거부] 로그인 실패: id=%s\n", loginId.c_str());
        SendLoginFailure(session, ToLoginResult(authResult));
        return true;   // 연결은 유지한다. 비번을 고쳐 다시 시도할 수 있어야 한다
    }

    // 중복 접속 거부. 허용하면 같은 계정이 두 세션으로 존재해서 귓속말이 어느 쪽으로
    // 갈지 정할 수 없고, 나중에 월드 상태가 붙으면 같은 캐릭터가 두 군데 있게 된다.
    if (gSessions.IsUserOnline(accountId)) {
        std::printf("[거부] 이미 접속 중인 계정: %s\n", loginId.c_str());
        SendLoginFailure(session, LoginResult::AlreadyOnline);
        return true;
    }

    net::LoginAckBody ack{};
    ack.UserId        = accountId;
    ack.bSuccess      = 1;
    ack.Result        = static_cast<uint8_t>(LoginResult::Success);
    ack.ServerVersion = net::kProtocolVersion;
    net::CopyFixedString(ack.Nickname, net::kMaxNameLen, nickname);

    // ★ Ack 를 먼저 보내고 authed 를 세운다.
    //   순서를 바꾸면 그 틈에 다른 스레드의 브로드캐스트가 이 소켓으로 나가서,
    //   클라이언트가 로그인 응답보다 먼저 채팅을 받는다.
    SendTo(session, Opcode::LoginAck, &ack, sizeof(ack));

    session->userId   = accountId;
    session->nickname = nickname;
    session->authed   = true;

    std::printf("[로그인] %s -> UserId=%llu (%s)\n",
                nickname.c_str(), static_cast<unsigned long long>(accountId),
                session->peerAddress.c_str());

    BroadcastSystem(nickname + " 님이 접속했습니다.", session);
    SendSystemTo(session, "GuideStory 채팅에 연결되었습니다. (/w 닉네임 내용 = 귓속말)");
    return true;
}

// 계정 생성. 로그인과 달리 세션 상태를 바꾸지 않는다.
// 가입에 성공해도 자동 로그인은 시키지 않고, 클라이언트가 이어서 LoginReq 를 보낸다.
// 둘을 분리해두면 나중에 "가입 즉시 이메일 인증" 같은 단계를 끼우기 쉽다.
bool HandleRegisterReq(const SessionPtr& session, const char* body, uint32_t bodySize) {
    auto sendResult = [&](LoginResult reason) {
        net::RegisterAckBody ack{};
        ack.bSuccess      = (reason == LoginResult::Success) ? 1 : 0;
        ack.Result        = static_cast<uint8_t>(reason);
        ack.ServerVersion = net::kProtocolVersion;
        SendTo(session, Opcode::RegisterAck, &ack, sizeof(ack));
        return true;
    };

    if (bodySize < sizeof(uint16_t)) return sendResult(LoginResult::InvalidRequest);

    uint16_t clientVersion = 0;
    std::memcpy(&clientVersion, body, sizeof(clientVersion));
    if (clientVersion != net::kProtocolVersion) {
        std::printf("[거부] 가입 요청 버전 불일치. 클라이언트=%u, 서버=%u\n",
                    clientVersion, net::kProtocolVersion);
        return sendResult(LoginResult::VersionMismatch);
    }

    if (bodySize < sizeof(net::RegisterReqBody)) return sendResult(LoginResult::InvalidRequest);

    net::RegisterReqBody req{};
    std::memcpy(&req, body, sizeof(req));

    const std::string loginId  = net::ReadFixedString(req.LoginId,  net::kMaxLoginIdLen);
    const std::string password = net::ReadFixedString(req.Password, net::kMaxPasswordLen);
    const std::string nickname = net::ReadFixedString(req.Nickname, net::kMaxNameLen);

    uint64_t newUserId = 0;
    const AccountResult r = accounts::Create(loginId, password, nickname, newUserId);

    if (r == AccountResult::Success) {
        std::printf("[가입] %s (닉네임 %s) -> UserId=%llu\n",
                    loginId.c_str(), nickname.c_str(),
                    static_cast<unsigned long long>(newUserId));
    } else {
        std::printf("[거부] 가입 실패: id=%s 사유=%s\n",
                    loginId.c_str(), net::LoginResultText(ToLoginResult(r)));
    }

    return sendResult(ToLoginResult(r));
}

bool HandleChatSend(const SessionPtr& session, const char* body, uint32_t bodySize) {
    if (!session->authed) return true;   // 로그인 전 채팅은 무시하되 연결은 유지
    if (bodySize < sizeof(net::ChatSendBody)) return false;

    net::ChatSendBody req{};
    std::memcpy(&req, body, sizeof(req));

    // 선언한 TextLen 과 실제 도착한 바이트 수가 맞는지 확인한다.
    // 이 검사가 없으면 TextLen 을 크게 속여 버퍼 밖을 읽게 만들 수 있다.
    if (req.TextLen > net::kMaxTextLen) return false;
    if (sizeof(net::ChatSendBody) + req.TextLen != bodySize) return false;

    const char* text = body + sizeof(net::ChatSendBody);
    const std::string targetName = net::ReadFixedString(req.TargetName, net::kMaxNameLen);
    RouteChat(session, static_cast<ChatChannel>(req.Channel), targetName, text, req.TextLen);
    return true;
}

bool HandlePacket(const SessionPtr& session, const net::PacketHeader& header,
                  const std::vector<char>& body) {
    const char* data = body.empty() ? nullptr : body.data();
    const uint32_t size = static_cast<uint32_t>(body.size());

    switch (static_cast<Opcode>(header.Opcode)) {
        case Opcode::LoginReq:    return HandleLoginReq(session, data, size);
        case Opcode::RegisterReq: return HandleRegisterReq(session, data, size);
        case Opcode::ChatSend:    return HandleChatSend(session, data, size);
        case Opcode::Heartbeat:
            // 그대로 돌려준다. 클라이언트는 이걸 받아 "서버가 살아있다" 를 안다.
            SendTo(session, Opcode::Heartbeat, nullptr, 0);
            return true;
        default:
            std::printf("[경고] 알 수 없는 오피코드 %u\n", header.Opcode);
            return false;
    }
}

// ---------------------------------------------------------------------------
// 클라이언트 스레드 (접속 1건당 1개)
// ---------------------------------------------------------------------------
void ClientThread(SessionPtr session) {
    char temp[1024];
    net::PacketHeader header{};
    std::vector<char> body;

    for (;;) {
        const int received = ::recv(session->sock, temp, sizeof(temp), 0);

        // 0 이면 정상 종료, 음수면 에러. != 0 만 보면 에러(-1) 일 때 -1 을 길이로 쓰게 된다.
        if (received <= 0) break;

        session->recvBuf.insert(session->recvBuf.end(), temp, temp + received);

        // 한 번의 recv 에 여러 패킷이 붙어 왔을 수 있으므로 다 꺼낼 때까지 돈다.
        bool disconnect = false;
        for (;;) {
            const net::FrameResult result = net::TryExtractPacket(session->recvBuf, header, body);

            if (result == net::FrameResult::NeedMore) break;
            if (result == net::FrameResult::Malformed) {
                std::printf("[차단] 비정상 패킷 크기. 연결을 끊는다. (UserId=%llu)\n",
                            static_cast<unsigned long long>(session->userId));
                disconnect = true;
                break;
            }
            if (!HandlePacket(session, header, body)) {
                disconnect = true;
                break;
            }
        }

        if (disconnect) break;
    }

    // 정상 종료든 랜선이 뽑혔든 이 자리를 지나가므로 퇴장 처리는 한 곳에서 끝난다.
    // ★ Remove 보다 먼저 알린다 — 목록에서 빠진 뒤에는 남들에게 보낼 수 없다.
    if (session->authed) {
        session->authed = false;   // 자기 자신에게 퇴장 알림이 가지 않게 먼저 내린다
        BroadcastSystem(session->nickname + " 님이 나갔습니다.");
    }

    std::printf("[종료] %s 연결 해제\n",
                session->nickname.empty() ? "(미로그인)" : session->nickname.c_str());

    gSessions.Remove(session);
    std::printf("       현재 접속자 %zu명\n", gSessions.Count());
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);   // 콘솔에 한글 로그를 그대로 찍기 위해
#endif
    // 출력을 파일로 리다이렉트하면 stdout 이 전체 버퍼링으로 바뀌어
    // 프로세스가 끝날 때까지 로그가 하나도 보이지 않는다.
    // MSVC 는 _IOLBF(줄 버퍼링)를 _IOFBF 와 동일하게 처리하므로 무버퍼로 둔다.
    ::setvbuf(stdout, nullptr, _IONBF, 0);

    if (argc > 3) {
        std::printf("사용법: %s [port] [db경로]   (기본 7777 / guidestory.db)\n", argv[0]);
        return 1;
    }

    const uint16_t port = (argc >= 2)
        ? static_cast<uint16_t>(std::atoi(argv[1]))
        : 7777;
    const char* dbPath = (argc >= 3) ? argv[2] : "guidestory.db";

    if (port == 0) {
        std::printf("포트 번호가 올바르지 않습니다: %s\n", argv[1]);
        return 1;
    }

    if (!net::NetInit()) {
        std::printf("네트워크 초기화 실패 (WSAStartup)\n");
        return 1;
    }

    const net::SocketHandle listenSock = ::socket(PF_INET, SOCK_STREAM, 0);
    if (listenSock == net::kInvalidSocket) {
        std::printf("socket() 실패: %d\n", net::LastNetError());
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port        = htons(port);

    if (::bind(listenSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) != 0) {
        std::printf("bind() 실패: %d (포트 %u 를 이미 누가 쓰고 있는지 확인할 것)\n",
                    net::LastNetError(), port);
        return 1;
    }
    if (::listen(listenSock, SOMAXCONN) != 0) {
        std::printf("listen() 실패: %d\n", net::LastNetError());
        return 1;
    }

    std::signal(SIGINT,  OnInterrupt);
    std::signal(SIGTERM, OnInterrupt);

    // 계정 DB. 이게 없으면 아무도 로그인할 수 없으므로 실패는 치명적이다.
    if (!accounts::Start(dbPath)) {
        std::printf("[치명] 계정 DB 를 열지 못했다. 아무도 로그인할 수 없다.\n");
        return 1;
    }

    // 채팅 로그는 같은 파일에 둔다(테이블이 다르므로 섞이지 않는다).
    // 커넥션은 별개다 — 이쪽은 라이터 스레드 전용이라 남이 끼면 안 된다.
    // 열기에 실패해도 서버는 계속 돈다: 로그가 안 남는 것보다 채팅이 끊기는 게 나쁘다.
    if (!chatlog::Start(dbPath)) {
        std::printf("[경고] 채팅 로그를 열지 못했다. 채팅은 되지만 기록은 남지 않는다.\n");
    }

    std::printf("=== GuideStory 계정/채팅 서버 시작 (port %u, 프로토콜 v%u) ===\n",
                port, net::kProtocolVersion);

    while (gRunning) {
        sockaddr_in clientAddr{};
        int addrSize = sizeof(clientAddr);

        const net::SocketHandle clientSock =
            ::accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr),
#ifdef _WIN32
                     &addrSize);
#else
                     reinterpret_cast<socklen_t*>(&addrSize));
#endif
        if (clientSock == net::kInvalidSocket) {
            std::printf("accept() 실패: %d\n", net::LastNetError());
            continue;
        }

        char addrText[INET_ADDRSTRLEN] = {};
        ::inet_ntop(AF_INET, &clientAddr.sin_addr, addrText, sizeof(addrText));
        std::printf("[접속] %s\n", addrText);

        // 고정 배열이 아니므로 접속자 수 상한이 없다.
        SessionPtr session = gSessions.Add(clientSock);
        session->peerAddress = addrText;
        std::thread(ClientThread, session).detach();
    }

    chatlog::Stop();
    accounts::Stop();
    net::CloseSocket(listenSock);
    net::NetShutdown();
    return 0;
}
