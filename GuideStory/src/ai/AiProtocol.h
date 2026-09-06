#pragma once

#include <cstdint>

// AI 추론 채널의 와이어 규약. **파이썬 쪽 GuideStoryAI/serve/ai_server.py 와 짝이다.**
//
// 헤더 레이아웃은 net::PacketHeader 를 그대로 쓴다(6바이트, BodySize + Opcode, pack(1)).
// 그래서 net/Framing 의 SendAll·BuildPacket·TryExtractPacket 을 새로 짜지 않고 재사용한다 —
// TCP 경계 문제를 이미 한 번 풀어놨고, 두 번 푸는 것은 두 번 틀릴 기회다.
//
// **두 가지는 의도적으로 다르다.**
//   1) 옵코드 공간이 독립이다. 여기는 다른 포트의 다른 엔드포인트이므로 계정/채팅 옵코드와
//      번호를 공유할 이유가 없다. 헤더 레이아웃만 같으면 프레이밍 코드는 공유된다.
//   2) 바디 상한이 1 MiB 다. net::kMaxBodySize 는 4 KiB인데 그건 채팅 한 줄 기준이고,
//      32x32 격자 JSON은 그걸 넘는다. **상한을 검사한다는 원칙은 같고 값만 다르다.**
//
// [주의] 이 헤더는 소켓 타입을 포함하지 않는다. net/Socket.h 는 winsock2.h 를 끌고 오는데
//        그게 SDL 을 쓰는 TU 에 섞이면 재정의 오류가 난다(net/NetClient.h 와 같은 이유).
//        소켓은 RemoteShapeGenerator.cpp 안에만 존재한다.
namespace gs::ai {

// 바디(JSON) 안의 "v" 필드. 요청/응답 구조가 바뀌면 올린다.
//   1 : generate / ping — 격자를 row-major 평탄 배열로 주고받는다
inline constexpr uint16_t kAiProtocolVersion = 1;

// 선언된 길이를 신뢰하기 전에 검사할 상한. 128x128 격자 JSON에 여유를 둔 값이다.
inline constexpr uint32_t kAiMaxBodySize = 1u << 20; // 1 MiB

inline constexpr uint16_t kDefaultAiPort = 7788;

// AI 채널 전용 옵코드. 계정 서버와 같은 규칙 — **항상 끝에 추가한다.**
// 중간에 끼우면 뒤 번호가 밀려서 파이썬 쪽과 조용히 어긋난다(요청은 도착하는데 다른 핸들러가 받는다).
enum class AiOpcode : uint16_t {
    None        = 0,
    GenerateReq = 1,
    GenerateAck = 2,
    PingReq     = 3,
    PingAck     = 4,
};

// 요청이 지정할 수 있는 격자 크기. 서버도 같은 범위를 검사한다(양쪽 다 상대를 신뢰하지 않는다).
inline constexpr int kMinGridDim = 4;
inline constexpr int kMaxGridDim = 128;

} // namespace gs::ai
