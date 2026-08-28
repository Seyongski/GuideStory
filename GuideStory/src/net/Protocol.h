// GuideStory 계정/채팅 서버 <-> 클라이언트 공용 프로토콜 정의.
//
// 이 파일은 서버(GuideStoryServer)와 클라이언트(GuideStoryGame)가 **같은 소스를 컴파일**한다.
// 한쪽만 고치면 조용히 어긋나므로, 헤더 하나를 공유해서 그럴 여지를 없앤다.
//
// 그래서 여기서는 SDL·WinSock·프로젝트 헤더에 의존하지 않는다. <cstdint> 만 쓴다.
// (ADR-006: net 은 SDL 네이티브 타입에 의존하지 않는다.)
//
// 출처: Unreal-MOU/MOU_Server/Shared/ChatProtocol.h 를 GuideStory 에 맞게 재설계했다.
//   가져온 것 : 길이 프리픽스 헤더, 버전 협상(첫 필드 Version), 고정 크기 바디 + 가변 텍스트,
//              #pragma pack(1) + static_assert 로 패딩 사고를 컴파일 타임에 잡는 방식.
//   버린 것   : 팀(TeamId)·사망 채널·로비/대기실/방 — 전부 4인 리슨서버 co-op 전제의 개념이라
//              지속 월드(ADR-001 데디케이트 서버)를 지향하는 GuideStory 에는 맞지 않는다.
//              "안 쓰는 옵코드를 일단 옮겨두기" 는 죽은 프로토콜을 만들 뿐이다.
#pragma once

#include <cstdint>

namespace gs::net {

// 헤더 구조나 옵코드 의미가 바뀌면 올린다.
// 로그인/가입 시점에 서버가 이 값을 검사하고, 다르면 명확한 사유와 함께 거부한다.
// 이게 없으면 서버만 업데이트했을 때 클라이언트가 원인 모를 실패를 반복한다.
//
//   1 : 계정(가입/로그인) + 채팅(전체/귓속말/시스템) + 하트비트
constexpr uint16_t kProtocolVersion = 1;

// BodySize 가 이 값을 넘으면 악성 패킷으로 보고 연결을 끊는다.
// 이 검사가 없으면 클라가 BodySize 에 큰 값을 적어 서버 메모리를 터뜨릴 수 있다.
constexpr uint32_t kMaxBodySize = 4096;

constexpr uint32_t kMaxNameLen = 32;   // 닉네임 (화면에 보이는 이름). 널 종료 포함
constexpr uint32_t kMaxTextLen = 512;  // 채팅 한 줄 본문(UTF-8 바이트)

// 로그인 아이디/비밀번호의 최대 바이트 수 (UTF-8 기준, 널 종료 포함).
constexpr uint32_t kMaxLoginIdLen  = 24;
constexpr uint32_t kMaxPasswordLen = 64;

// 계정 정책. 서버가 검사하고, 클라이언트는 미리 걸러서 왕복을 아낀다.
// (클라 검사는 편의일 뿐 신뢰의 근거가 아니다 — 판정은 언제나 서버가 한다.)
constexpr uint32_t kMinLoginIdLen  = 3;
constexpr uint32_t kMinPasswordLen = 6;
constexpr uint32_t kMinNicknameLen = 2;

enum class Opcode : uint16_t {
    None          = 0,
    LoginReq      = 1,
    LoginAck      = 2,
    RegisterReq   = 3,
    RegisterAck   = 4,
    ChatSend      = 5,
    ChatBroadcast = 6,
    Heartbeat     = 7,

    // 새 옵코드는 **항상 끝에 붙인다.** 중간에 끼우면 뒤 번호가 전부 밀려서,
    // 업데이트를 안 한 쪽과 조용히 어긋난다(패킷은 도착하는데 엉뚱한 핸들러가 받는다).
    // 쓰지 않게 된 번호도 지우지 않고 남겨두는 이유가 같다.
};

// 채팅 채널.
//
// 맵/파티 채널을 지금 넣지 않은 이유: 이 서버는 아직 "누가 어느 맵에 있는가" 를 모른다.
// 라우팅할 근거가 없는 채널을 프로토콜에만 선언해두면 **동작하지 않는 UI** 가 생긴다.
// 서버가 월드 상태를 갖는 시점(ADR-001 3단계)에 옵코드 뒤에 붙여서 늘린다.
enum class ChatChannel : uint8_t {
    All     = 0,   // 전체 — 접속한 모든 인증 세션
    Whisper = 1,   // 귓속말 — 닉네임으로 지정한 한 명
    System  = 2,   // 시스템 — 서버만 만든다. 클라가 보내면 거부한다
};

// 로그인/가입 결과. LoginAckBody / RegisterAckBody 의 Result 에 담겨 돌아온다.
// 클라이언트는 이 값을 보고 "다시 시도해도 소용없는 실패" 인지 판단한다.
enum class LoginResult : uint8_t {
    Success         = 0,
    VersionMismatch = 1,   // 클라와 서버의 kProtocolVersion 이 다르다. 재시도해도 계속 실패한다
    InvalidRequest  = 2,   // 바디 크기가 맞지 않는다
    AccountNotFound = 3,   // 그런 아이디가 없다
    WrongPassword   = 4,
    DuplicateId     = 5,   // 가입하려는 아이디가 이미 있다
    DuplicateNick   = 6,   // 가입하려는 닉네임이 이미 있다
    InvalidFormat   = 7,   // 아이디/비번/닉 길이 규칙 위반
    AlreadyOnline   = 8,   // 같은 계정이 이미 접속해 있다
    ServerError     = 9,   // DB 오류 등 서버 문제. 클라이언트 잘못이 아니다
};

// 사유 코드를 사람이 읽는 한 줄로. 서버 로그와 클라 화면이 같은 문장을 쓰게 해서
// "화면에는 로그인 실패만 뜨고 원인은 서버 콘솔에만 있는" 상황을 막는다.
inline const char* LoginResultText(LoginResult r) {
    switch (r) {
        case LoginResult::Success:         return "성공";
        case LoginResult::VersionMismatch: return "서버와 버전이 다릅니다 (양쪽을 다시 빌드하세요)";
        case LoginResult::InvalidRequest:  return "잘못된 요청입니다";
        case LoginResult::AccountNotFound: return "없는 아이디입니다";
        case LoginResult::WrongPassword:   return "비밀번호가 틀렸습니다";
        case LoginResult::DuplicateId:     return "이미 있는 아이디입니다";
        case LoginResult::DuplicateNick:   return "이미 있는 닉네임입니다";
        case LoginResult::InvalidFormat:   return "아이디/비밀번호/닉네임 형식이 올바르지 않습니다";
        case LoginResult::AlreadyOnline:   return "이미 접속 중인 계정입니다";
        case LoginResult::ServerError:     return "서버 오류입니다";
        default:                           return "알 수 없는 오류입니다";
    }
}

inline const char* ChannelName(ChatChannel c) {
    switch (c) {
        case ChatChannel::All:     return "전체";
        case ChatChannel::Whisper: return "귓속말";
        case ChatChannel::System:  return "시스템";
        default:                   return "알수없음";
    }
}

#pragma pack(push, 1)

// 모든 패킷 앞에 붙는 고정 헤더. BodySize 는 이 헤더를 제외한 페이로드 크기다.
// TCP 는 메시지 경계를 보장하지 않으므로 수신측이 이 길이로 직접 잘라야 한다(Framing.h).
struct PacketHeader {
    uint32_t BodySize;
    uint16_t Opcode;
};

// [경고] Password 는 평문으로 전송된다.
//   서버가 저장할 때는 솔트 + PBKDF2 로 해시하지만(Crypto.h), 전송 구간에는 암호화가 없다.
//   같은 네트워크에 있는 사람이 패킷을 뜨면 비밀번호가 그대로 보인다.
//   >> 실제로 쓰는 비밀번호를 여기에 넣지 말 것. <<
//   제대로 하려면 TLS 를 씌워야 한다 — tech-debt-tracker.md D-008.
struct LoginReqBody {
    // Version 은 반드시 첫 필드여야 한다.
    // 구조체 전체 크기가 서로 달라도 서버가 이 2바이트만은 읽을 수 있어야
    // "버전이 안 맞다" 고 정확히 알려줄 수 있기 때문이다.
    // 필드를 추가할 때는 반드시 뒤에 붙이고 Version 은 그대로 둔다.
    uint16_t Version;
    char     LoginId[kMaxLoginIdLen];
    char     Password[kMaxPasswordLen];
};

struct LoginAckBody {
    uint64_t UserId;                 // accounts.id. 실패면 0. 재접속해도 같은 번호다
    char     Nickname[kMaxNameLen];  // 서버가 확정한 이름 (클라가 보낸 값을 쓰지 않는다)
    uint8_t  bSuccess;
    uint8_t  Result;                 // LoginResult
    uint16_t ServerVersion;          // 버전 불일치 시 어느 쪽이 낡았는지 바로 보이게
};

// 가입. 로그인과 같은 이유로 Version 이 첫 필드다.
struct RegisterReqBody {
    uint16_t Version;
    char     LoginId[kMaxLoginIdLen];
    char     Password[kMaxPasswordLen];
    char     Nickname[kMaxNameLen];
};

struct RegisterAckBody {
    uint8_t  bSuccess;
    uint8_t  Result;                 // LoginResult
    uint16_t ServerVersion;
};

// 뒤에 TextLen 바이트의 UTF-8 본문이 이어진다.
//
// [대상을 UserId 가 아니라 닉네임으로 받는 이유]
//   사용자는 "/w 닉 안녕" 이라고 친다. UserId 로 받으면 클라가 닉->id 를 알아내려고
//   한 번 더 왕복해야 하는데, 그 조회를 할 수 있는 것은 결국 서버뿐이다.
//   서버가 이름을 푸는 편이 왕복이 하나 적고 실패 사유(그런 사람 없음)도 한곳에서 나온다.
struct ChatSendBody {
    char     TargetName[kMaxNameLen];   // Whisper 전용. 그 외에는 빈 문자열
    uint16_t TextLen;
    uint8_t  Channel;                   // ChatChannel
};

// 뒤에 TextLen 바이트의 UTF-8 본문이 이어진다.
// SenderUserId / SenderName 은 **서버가 세션 정보로 채운다.**
// 클라이언트가 보낸 값을 그대로 옮기지 않는다 — 그러면 남을 사칭할 수 있다.
struct ChatBroadcastBody {
    uint64_t SenderUserId;
    int64_t  Timestamp;                 // Unix epoch (초)
    char     SenderName[kMaxNameLen];
    uint16_t TextLen;
    uint8_t  Channel;                   // ChatChannel
};

#pragma pack(pop)

// 패딩이 끼면 서버와 클라이언트의 해석이 어긋난다(필드가 통째로 밀린다).
// #pragma pack(1) 이 빠지거나 필드 순서를 바꿨을 때 여기서 빌드가 깨진다.
static_assert(sizeof(PacketHeader)      ==   6, "PacketHeader 는 6바이트여야 한다");
static_assert(sizeof(LoginReqBody)      ==  90, "LoginReqBody 에 패딩이 끼었다");
static_assert(sizeof(LoginAckBody)      ==  44, "LoginAckBody 에 패딩이 끼었다");
static_assert(sizeof(RegisterReqBody)   == 122, "RegisterReqBody 에 패딩이 끼었다");
static_assert(sizeof(RegisterAckBody)   ==   4, "RegisterAckBody 에 패딩이 끼었다");
static_assert(sizeof(ChatSendBody)      ==  35, "ChatSendBody 에 패딩이 끼었다");
static_assert(sizeof(ChatBroadcastBody) ==  51, "ChatBroadcastBody 에 패딩이 끼었다");

// 채팅 한 줄이 한 패킷에 담기는지. 넘치면 kMaxTextLen 을 줄여야 한다.
static_assert(sizeof(ChatBroadcastBody) + kMaxTextLen <= kMaxBodySize,
              "채팅 한 줄이 kMaxBodySize 를 넘는다");

} // namespace gs::net
