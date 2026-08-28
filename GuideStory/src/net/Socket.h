// 플랫폼별 소켓 API 차이를 흡수하는 얇은 래퍼.
//
// ADR-006 과 같은 정신이다 — SDL 을 platform/ 뒤에 숨기듯, 소켓 API 를 여기 숨긴다.
// 서버 루프와 클라이언트 워커는 이 헤더의 이름만 쓰고 winsock2.h / sys/socket.h 를 직접 모른다.
// 그래서 나중에 리눅스 데디케이트 서버로 옮길 때 고칠 파일이 이 하나다.
//
// [주의 — 포함 순서]
//   winsock2.h 는 반드시 windows.h 보다 먼저 들어가야 한다(먼저 들어가면 winsock.h 구버전이
//   딸려와 재정의 오류가 난다). 그래서 이 헤더는 **SDL 을 쓰는 TU 에 포함하지 않는다** —
//   NetClient.h 도 이 헤더를 포함하지 않고 소켓을 .cpp 안에만 둔다.
//
// 출처: Unreal-MOU/MOU_Server/Shared/Net.h (거의 그대로. 네임스페이스만 gs::net).
#pragma once

#ifdef _WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")   // tech-debt D-005 상환: 링크 설정을 코드에 붙여둔다

namespace gs::net {

using SocketHandle = SOCKET;
inline constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

inline bool NetInit()     { WSADATA data; return ::WSAStartup(MAKEWORD(2, 2), &data) == 0; }
inline void NetShutdown() { ::WSACleanup(); }
inline void CloseSocket(SocketHandle s) { ::closesocket(s); }
inline int  LastNetError() { return ::WSAGetLastError(); }

// 보낼 데이터를 다 내보낸 뒤 FIN 을 보낸다. 수신은 계속 열어둔다.
// 다른 스레드가 recv() 에 블록된 소켓을 곧바로 closesocket() 하면
// 아직 나가지 않은 송신 데이터가 버려질 수 있다.
inline void ShutdownSend(SocketHandle s) { ::shutdown(s, SD_SEND); }

// recv() 가 영원히 블록되지 않게 한다. 이 타임아웃이 곧 "종료 요청에 대한 반응 속도" 다.
inline bool SetRecvTimeout(SocketHandle s, int milliseconds) {
    const DWORD timeout = static_cast<DWORD>(milliseconds);
    return ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
                        reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
}

inline bool IsRecvTimeout(int errorCode) { return errorCode == WSAETIMEDOUT; }

// 논블로킹 전환. connect() 를 타임아웃과 함께 걸기 위해 쓴다(ConnectWithTimeout 참고).
inline bool SetNonBlocking(SocketHandle s, bool on) {
    u_long mode = on ? 1u : 0u;
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
}

// 논블로킹 connect() 가 "아직 진행 중" 이라고 말하는 코드. 실패가 아니다.
inline bool IsConnectPending(int errorCode) { return errorCode == WSAEWOULDBLOCK; }

} // namespace gs::net

#else

    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <cerrno>

namespace gs::net {

using SocketHandle = int;
inline constexpr SocketHandle kInvalidSocket = -1;

inline bool NetInit()     { return true; }
inline void NetShutdown() {}
inline void CloseSocket(SocketHandle s) { ::close(s); }
inline int  LastNetError() { return errno; }

inline void ShutdownSend(SocketHandle s) { ::shutdown(s, SHUT_WR); }

inline bool SetRecvTimeout(SocketHandle s, int milliseconds) {
    timeval timeout{};
    timeout.tv_sec  = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    return ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
}

inline bool IsRecvTimeout(int errorCode) {
    return errorCode == EAGAIN || errorCode == EWOULDBLOCK;
}

inline bool SetNonBlocking(SocketHandle s, bool on) {
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) return false;
    flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return ::fcntl(s, F_SETFL, flags) == 0;
}

inline bool IsConnectPending(int errorCode) { return errorCode == EINPROGRESS; }

} // namespace gs::net

#endif
