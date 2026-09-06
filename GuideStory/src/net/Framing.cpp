#include "net/Framing.h"

#include <algorithm>
#include <cstring>

namespace gs::net {

bool SendAll(SocketHandle sock, const char* data, int32_t len) {
    int32_t sent = 0;
    while (sent < len) {
        const int written = ::send(sock, data + sent, len - sent, 0);
        if (written <= 0) return false; // 연결이 끊겼거나 에러
        sent += written;
    }
    return true;
}

std::vector<char> BuildPacket(Opcode op,
                              const void* bodyA, uint32_t sizeA,
                              const void* bodyB, uint32_t sizeB,
                              uint32_t maxBodySize) {
    const uint32_t total = sizeA + sizeB;
    if (total > maxBodySize) return {}; // 호출측이 빈 버퍼를 보고 실패를 안다

    std::vector<char> buffer(sizeof(PacketHeader) + total);

    PacketHeader header{};
    header.BodySize = total;
    header.Opcode   = static_cast<uint16_t>(op);
    std::memcpy(buffer.data(), &header, sizeof(header));

    if (sizeA > 0 && bodyA) std::memcpy(buffer.data() + sizeof(header), bodyA, sizeA);
    if (sizeB > 0 && bodyB) std::memcpy(buffer.data() + sizeof(header) + sizeA, bodyB, sizeB);
    return buffer;
}

bool SendPacket2(SocketHandle sock, Opcode op,
                 const void* bodyA, uint32_t sizeA,
                 const void* bodyB, uint32_t sizeB) {
    const std::vector<char> buffer = BuildPacket(op, bodyA, sizeA, bodyB, sizeB);
    if (buffer.empty()) return false; // kMaxBodySize 초과
    return SendAll(sock, buffer.data(), static_cast<int32_t>(buffer.size()));
}

bool SendPacket(SocketHandle sock, Opcode op, const void* body, uint32_t bodySize) {
    return SendPacket2(sock, op, body, bodySize, nullptr, 0);
}

FrameResult TryExtractPacket(std::vector<char>& buffer,
                             PacketHeader& outHeader,
                             std::vector<char>& outBody,
                             uint32_t maxBodySize) {
    // 1. 헤더조차 다 안 왔으면 더 기다린다.
    if (buffer.size() < sizeof(PacketHeader)) return FrameResult::NeedMore;

    PacketHeader header{};
    std::memcpy(&header, buffer.data(), sizeof(header));

    // 2. 선언된 길이를 신뢰하기 전에 상한을 검사한다.
    //    이 검사가 없으면 악성 클라가 BodySize 에 큰 값을 넣어 메모리를 폭발시킬 수 있다.
    //    (RELIABILITY.md §2.2 — 비정상 패킷은 거부·로깅한다.)
    if (header.BodySize > maxBodySize) return FrameResult::Malformed;

    // 3. 바디가 덜 왔으면 더 기다린다. 지금까지 받은 건 버리지 않는다.
    const size_t total = sizeof(PacketHeader) + header.BodySize;
    if (buffer.size() < total) return FrameResult::NeedMore;

    // 4. 딱 한 패킷만 꺼내고 그만큼만 버퍼에서 지운다.
    outHeader = header;
    outBody.assign(buffer.begin() + sizeof(PacketHeader), buffer.begin() + total);
    buffer.erase(buffer.begin(), buffer.begin() + total);
    return FrameResult::Ok;
}

void CopyFixedString(char* dest, size_t destSize, const std::string& src) {
    if (destSize == 0) return;
    const size_t count = std::min(src.size(), destSize - 1);
    std::memcpy(dest, src.data(), count);
    dest[count] = '\0';
}

std::string ReadFixedString(const char* src, size_t maxSize) {
    size_t len = 0;
    while (len < maxSize && src[len] != '\0') ++len;
    return std::string(src, len);
}

} // namespace gs::net
