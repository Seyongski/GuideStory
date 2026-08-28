// 계정 저장소 (아이디 / 비밀번호 / 닉네임) — SQLite.
//
// [ChatLog 와 무엇이 다른가 — 중요]
//   ChatLog 는 비동기 큐다. 채팅 한 줄이 늦게 저장되거나 서버가 죽어 몇 줄 유실돼도
//   게임이 망가지지 않기 때문에, 지연을 없애는 쪽을 택했다.
//
//   계정은 정반대다. "가입 버튼을 눌렀는데 서버가 죽어서 계정이 없다" 는 있을 수 없다.
//   그래서 여기서는 호출한 스레드에서 그 자리에서 쓰고 커밋한다(동기).
//   로그인은 자주 일어나는 일이 아니라 이 정도 지연은 문제되지 않는다.
//
// [커넥션을 따로 쓰는 이유]
//   ChatLog 의 sqlite3* 는 라이터 스레드 전용이다. 남이 끼어들면 그 전제가 깨진다.
//   여기서는 여러 클라이언트 스레드가 동시에 로그인할 수 있으므로
//   자체 커넥션 + 뮤텍스로 직렬화한다.
//
// [UserId 의 의미]
//   UserId = accounts.id 다. 접속 일련번호가 아니라 계정 번호이므로,
//   서버를 재시작해도 같은 계정이면 언제나 같은 번호다(채팅 로그의 sender_id 가 의미를 갖는다).
//
// 출처: Unreal-MOU/MOU_Server/Server/Accounts.h.
//   달라진 점: 닉네임에도 UNIQUE 를 걸어 중복 닉을 거부한다(프로토콜의 DuplicateNick).
//   MOU 는 4인 co-op 이라 동명이인이 문제되지 않았지만, GuideStory 는 귓속말 대상을
//   닉네임으로 지정한다(Protocol.h ChatSendBody) — 동명이인이 있으면 누구에게 갈지 정할 수 없다.
#pragma once

#include <cstdint>
#include <string>

namespace gs::server {

// 계정 작업 결과. Server.cpp 가 프로토콜의 LoginResult 로 옮겨 클라이언트에 보낸다.
enum class AccountResult : uint8_t {
    Success = 0,
    NotFound,        // 아이디 없음
    WrongPassword,
    DuplicateId,     // 가입 시 이미 있는 아이디
    DuplicateNick,   // 가입 시 이미 있는 닉네임
    InvalidFormat,   // 길이 규칙 위반
    DbError,
};

namespace accounts {

// 계정 DB 를 연다. 서버 시작 시 한 번 부른다.
// 채팅 로그와 같은 파일을 써도 되고 달라도 된다(테이블이 다르므로 섞이지 않는다).
bool Start(const char* dbPath);

// DB 를 닫는다. Start 가 실패했어도 부르는 것이 안전하다.
void Stop();

// 계정을 만든다. 성공하면 outUserId 에 새 계정 번호가 담긴다.
// 이 함수가 돌아온 시점에는 이미 디스크에 커밋되어 있다.
AccountResult Create(const std::string& loginId, const std::string& password,
                     const std::string& nickname, uint64_t& outUserId);

// 아이디/비밀번호를 검증한다. 성공 시 outUserId 와 outNickname 이 채워진다.
//
// 실패 사유로 NotFound 와 WrongPassword 를 구분해 돌려주는데, 이건 "어느 아이디가
// 존재하는지" 를 외부에 알려주는 것이라 보안상 손해다. 다만 개발 중 진단 편의를 택했다.
// 외부 서비스로 낼 거라면 둘 다 같은 사유로 뭉뚱그려야 한다.
AccountResult Authenticate(const std::string& loginId, const std::string& password,
                           uint64_t& outUserId, std::string& outNickname);

} // namespace accounts
} // namespace gs::server
