// 길이 프리픽스 프레이밍 — 서버와 클라이언트가 공유한다.
//
// TCP 는 바이트 스트림이라 send() 한 번이 recv() 한 번으로 오지 않는다.
// 두 패킷이 붙어서 오기도 하고(합침), 하나가 쪼개져 오기도 한다(분할).
// 그래서 PacketHeader.BodySize 를 보고 경계를 직접 잘라야 한다.
//
// 출처: Unreal-MOU/MOU_Server/Shared/Framing.h (그대로. 네임스페이스/명명 규칙만 GuideStory 식).
#pragma once

#include "net/Protocol.h"
#include "net/Socket.h"

#include <string>
#include <vector>

namespace gs::net {

enum class FrameResult : uint8_t {
    Ok,        // 완성된 패킷 하나를 꺼냈다
    NeedMore,  // 아직 덜 왔다. 다음 recv 를 기다린다
    Malformed, // BodySize 가 허용치를 넘었다. 연결을 끊어야 한다
};

// send() 는 요청한 길이만큼 다 보내지 않을 수 있으므로 전부 나갈 때까지 반복한다.
bool SendAll(SocketHandle sock, const char* data, int32_t len);

// 헤더 + 바디를 하나의 버퍼로 합쳐 한 번에 보낸다.
// 헤더와 바디를 따로 send 하면 그 사이에 다른 스레드의 send 가 끼어들어 스트림이 섞인다.
bool SendPacket(SocketHandle sock, Opcode op, const void* body, uint32_t bodySize);

// 바디가 두 조각인 경우(고정부 + 가변 길이 텍스트). 채팅이 이 형태다.
bool SendPacket2(SocketHandle sock, Opcode op,
                 const void* bodyA, uint32_t sizeA,
                 const void* bodyB, uint32_t sizeB);

// 위와 같은 조립을 소켓 없이 버퍼로만 한다. 클라이언트가 게임 스레드에서 패킷을 만들어
// 워커 스레드의 송신 큐에 넣을 때 쓴다(게임 스레드는 소켓을 만지지 않는다).
std::vector<char> BuildPacket(Opcode op,
                              const void* bodyA, uint32_t sizeA,
                              const void* bodyB = nullptr, uint32_t sizeB = 0);

// buffer 앞쪽에서 완성된 패킷 하나를 꺼내 outHeader / outBody 에 담고,
// 꺼낸 만큼 buffer 앞부분을 지운다. 뒤에 남은 바이트는 다음 패킷의 일부이므로 보존한다.
// Ok 가 반환되는 동안 반복 호출해서 밀린 패킷을 전부 처리해야 한다.
FrameResult TryExtractPacket(std::vector<char>& buffer,
                             PacketHeader& outHeader,
                             std::vector<char>& outBody);

// 고정 길이 char 배열에 문자열을 담는다. 항상 널 종료를 보장한다(넘치면 자른다).
void CopyFixedString(char* dest, size_t destSize, const std::string& src);

// 상대가 널 종료를 빼먹었을 수 있으므로 길이를 제한해서 읽는다.
std::string ReadFixedString(const char* src, size_t maxSize);

} // namespace gs::net
