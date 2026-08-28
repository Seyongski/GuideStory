// 비밀번호 해시용 최소 암호 유틸 (SHA-256 / HMAC / PBKDF2).
//
// [왜 직접 구현하는가]
//   OpenSSL 을 붙이면 서버 하나 빌드하려고 의존성 트리를 통째로 복원해야 한다.
//   윈도우 전용 CNG(bcrypt.h)를 쓰면 서버가 윈도우에 묶인다 — net/Socket.h 는 이미
//   POSIX 분기를 갖고 있고, 리눅스 데디케이트로 옮기는 것이 ADR-001 의 방향이다.
//   필요한 게 SHA-256 하나뿐이라 표준 C++ 만으로 자체 구현했다.
//
// [왜 SHA-256 한 번이 아니라 PBKDF2 인가]
//   SHA-256 은 빠르라고 만든 함수다. GPU 로 초당 수십억 번 시도할 수 있어서
//   비밀번호를 그대로 해시하면 사전 공격에 사실상 무방비다.
//   PBKDF2 는 같은 해시를 수만 번 반복해서 "느리게" 만든다.
//   솔트는 같은 비밀번호가 같은 해시로 저장되는 것을 막는다(레인보우 테이블 방어).
//
// [한계 — 알고 쓸 것]
//   실무 표준은 Argon2id 나 bcrypt 다. PBKDF2 는 GPU 내성이 그들보다 약하다.
//   다만 "평문 저장" 이나 "솔트 없는 SHA-256" 과는 격이 다르고, 이 프로젝트 규모에서는 충분하다.
//
// 출처: Unreal-MOU/MOU_Server/Server/Crypto.h (알고리즘 그대로. 명명 규칙만 GuideStory 식).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace gs::server::crypto {

// SHA-256 다이제스트 크기(바이트).
constexpr std::size_t kSha256Size = 32;

// 저장에 쓰는 솔트 크기(바이트). 16바이트면 충돌 걱정이 없다.
constexpr std::size_t kSaltSize = 16;

// PBKDF2 반복 횟수.
//
// 크면 안전하지만 로그인 1회당 서버 CPU 를 그만큼 쓴다.
//
// [실측 — 이 구현, 10만 회, x64, 2026-08-25]
//   Release(/O2) 약 88ms, Debug(/Od) 약 404ms.
//   즉 한 코어가 초당 처리할 수 있는 로그인이 Release 기준 10여 건이다. 로그인은 접속당
//   한 번뿐이라 지금 규모에서는 충분하지만, **로그인 폭주는 그 자체로 CPU 고갈 공격이 된다**
//   (반복 횟수를 올릴수록 심해진다 — 안전과 가용성이 같은 손잡이의 양끝이다).
//   개발 중 Debug 빌드에서 로그인이 눈에 띄게 굼뜬 것은 이 값 때문이지 네트워크 탓이 아니다.
//   더 빠르게 해야 한다면 반복 횟수를 낮추는 것이 아니라 압축(SHA-256 라운드)을 최적화하거나
//   Argon2id 같은 최신 KDF 로 갈아타는 것이 맞다.
//
// >> 이 값을 바꾸면 기존 계정은 로그인할 수 없게 된다. <<
//    이미 저장된 해시는 옛 반복 횟수로 만들어졌기 때문이다.
//    바꿔야 한다면 accounts 테이블에 iterations 컬럼을 두고 계정마다 기록해야 한다.
constexpr uint32_t kPbkdf2Iterations = 100000;

// 임의 바이트를 채운다. 솔트 생성에 쓴다.
void RandomBytes(uint8_t* out, std::size_t len);

// 바이트열 -> 소문자 16진 문자열. DB 에 TEXT 로 넣기 위한 것이다.
std::string ToHex(const uint8_t* data, std::size_t len);

// 16진 문자열 -> 바이트열. 형식이 잘못되면 false.
bool FromHex(const std::string& hex, uint8_t* out, std::size_t outLen);

// SHA-256. out 은 kSha256Size 바이트여야 한다.
void Sha256(const uint8_t* data, std::size_t len, uint8_t* out);

// PBKDF2-HMAC-SHA256. out 은 kSha256Size 바이트여야 한다.
// 출력 길이를 해시 크기로 고정했다(dkLen = hLen 이라 블록이 하나뿐이다).
void Pbkdf2HmacSha256(const uint8_t* password, std::size_t passwordLen,
                      const uint8_t* salt, std::size_t saltLen,
                      uint32_t iterations, uint8_t* out);

// 두 바이트열을 상수 시간에 비교한다.
//
// memcmp 는 다른 바이트를 만나면 즉시 반환해서, 걸린 시간으로
// "앞에서 몇 바이트나 맞았는지" 를 추측당할 수 있다(타이밍 공격).
// 비밀 값 비교에는 반드시 이 함수를 쓴다.
bool ConstantTimeEquals(const uint8_t* a, const uint8_t* b, std::size_t len);

} // namespace gs::server::crypto
