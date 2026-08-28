// 채팅 로그 영속화 (SQLite, 비동기).
//
// [왜 비동기인가]
//   RouteChat 은 클라이언트 스레드 위에서 돈다. 거기서 곧바로 sqlite3_step 을 부르면
//   디스크 쓰기(특히 커밋 시 fsync)가 끝날 때까지 그 스레드가 멈춘다.
//   RouteChat 은 세션 목록 락 안에서 브로드캐스트를 하므로,
//   결국 "한 명이 채팅 칠 때마다 서버 전체 채팅이 디스크 대기만큼 밀리는" 구조가 된다.
//
//   그래서 쓰기를 한 스레드로 몰아 분리한다.
//     클라이언트 스레드 N개 --Enqueue--> [큐] --> DB 라이터 스레드 1개 --> chat_log.db
//   Enqueue 는 메모리 복사와 큐 push 만 하고 즉시 리턴하므로 채팅 지연이 생기지 않는다.
//
// [Accounts 와 반대로 간 이유]
//   계정은 유실되면 안 되므로 동기 + synchronous=FULL 이다(Accounts.h 주석).
//   채팅 로그는 비정상 종료 시 큐에 남은 몇 줄이 사라져도 게임이 망가지지 않는다.
//   같은 DB 파일을 쓰지만 보증 수준을 다르게 잡은 것이 이 둘의 핵심 차이다.
//
// [트레이드오프 — 알고 쓸 것]
//   큐에 있고 아직 커밋되지 않은 메시지는 서버가 비정상 종료하면 사라진다.
//   정상 종료(Stop) 시에는 남은 큐를 전부 비우고 닫으므로 유실이 없다.
//
// 출처: Unreal-MOU/MOU_Server/Server/ChatLog.h.
//   달라진 점: team_id 대신 target_name(귓속말 수신자)을 기록한다 — 팀 개념이 없고,
//   귓속말은 "누구에게 갔는가" 가 없으면 로그만 보고는 대화를 복원할 수 없다.
#pragma once

#include <cstdint>
#include <string>

namespace gs::server::chatlog {

// DB 를 열고 라이터 스레드를 띄운다. 서버 시작 시 한 번만 부른다.
//
// 실패해도 서버는 계속 돌아야 한다. 채팅 로그가 안 남는 것과
// 채팅 자체가 안 되는 것은 심각도가 다르기 때문이다.
// 실패 시 이후의 Enqueue 는 조용히 버려진다(Dropped 로 집계됨).
bool Start(const char* dbPath);

// 큐에 남은 것을 전부 쓰고 라이터 스레드를 정리한 뒤 DB 를 닫는다.
// Start 가 실패했더라도 부르는 것이 안전하다.
void Stop();

// 채팅 한 줄을 기록 대기열에 넣는다. 호출자를 막지 않는다.
//
// text 는 널 종료가 아닐 수 있으므로 길이를 따로 받는다
// (프로토콜상 ChatBroadcast 뒤에 붙는 UTF-8 본문이 그렇다).
// 내부에서 std::string 으로 복사하므로 호출자는 text 수명을 신경쓰지 않아도 된다.
//
// 큐가 상한에 닿으면 이 메시지를 버린다.
// 디스크가 느릴 때 메모리가 무한정 늘어나 서버가 죽는 것보다 낫다.
void Enqueue(int64_t timestamp, uint64_t senderUserId, const std::string& senderName,
             uint8_t channel, const std::string& targetName,
             const char* text, uint16_t textLen);

// 지금까지 실제로 DB 에 커밋된 줄 수.
uint64_t WrittenCount();

// 큐가 넘쳐서 버린 줄 수. 0 이 아니면 디스크가 못 따라가고 있다는 뜻이다.
uint64_t DroppedCount();

} // namespace gs::server::chatlog
