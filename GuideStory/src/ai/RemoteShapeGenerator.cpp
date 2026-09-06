#include "ai/RemoteShapeGenerator.h"

#include "ai/Json.h"
#include "net/Framing.h"
#include "net/Socket.h"   // winsock2.h — 이 .cpp 안에만 존재한다(헤더로 새어나가면 SDL과 충돌)

#include <chrono>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace gs::ai {

namespace {

constexpr int kConnectTimeoutMs = 1500;  // 서버가 없을 때 이만큼만 기다린다
constexpr int kRecvTimeoutMs    = 200;   // 종료 요청에 대한 반응 속도이기도 하다
constexpr int kReplyTimeoutMs   = 5000;  // 응답을 이보다 오래 기다리지 않는다
constexpr int kIdlePollMs       = 2000;  // 유휴 시 연결 상태를 다시 확인하는 주기

using Clock = std::chrono::steady_clock;

double MsSince(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// 요청 JSON 조립. 필드 이름은 GuideStoryAI/serve/ai_server.py 와 짝이다.
std::string BuildRequestJson(const ShapeRequest& req) {
    std::string out = "{\"v\":";
    out += std::to_string(kAiProtocolVersion);
    out += ",\"label\":" + json::Escape(ShapeLabelKey(req.label));
    out += ",\"w\":" + std::to_string(req.w);
    out += ",\"h\":" + std::to_string(req.h);
    out += ",\"tile\":" + std::to_string(static_cast<unsigned>(req.tile));
    out += ",\"seed\":" + std::to_string(req.seed);
    out += "}";
    return out;
}

ShapeResult MakeError(std::string why) {
    ShapeResult r;
    r.ok    = false;
    r.error = std::move(why);
    r.model = "remote";
    return r;
}

// 응답 JSON -> ShapeResult. **프로세스 밖에서 온 데이터**이므로 전부 검사한다.
ShapeResult ParseResponse(const std::vector<char>& body, const ShapeRequest& req) {
    json::Value root;
    std::string err;
    if (!json::Parse(std::string_view(body.data(), body.size()), root, err))
        return MakeError("응답 JSON 파싱 실패: " + err);
    if (!root.IsObject())
        return MakeError("응답이 JSON 객체가 아님");

    if (!root.GetBool("ok")) {
        std::string reason = root.GetString("error", "사유 없음");
        return MakeError("서버 거절: " + reason);
    }

    ShapeResult r;
    r.model       = root.GetString("model", "unknown");
    r.seed        = static_cast<unsigned int>(root.GetNumber("seed", 0.0));
    r.w           = root.GetInt("w", 0);
    r.h           = root.GetInt("h", 0);
    r.inferenceMs = root.GetNumber("inference_ms", 0.0);

    if (r.w < kMinGridDim || r.w > kMaxGridDim || r.h < kMinGridDim || r.h > kMaxGridDim)
        return MakeError("응답 격자 크기가 범위 밖: " + std::to_string(r.w) + "x" + std::to_string(r.h));

    // 요청한 크기와 다르면 에디터가 엉뚱한 영역에 붙인다 — 조용히 받아들이지 않는다.
    if (r.w != req.w || r.h != req.h)
        return MakeError("요청/응답 격자 크기 불일치");

    const json::Value* grid = root.Find("grid");
    if (grid == nullptr || !grid->IsArray())
        return MakeError("응답에 grid 배열이 없음");

    const std::size_t expected = static_cast<std::size_t>(r.w) * static_cast<std::size_t>(r.h);
    if (grid->items.size() != expected)
        return MakeError("grid 길이 불일치: " + std::to_string(grid->items.size()) +
                         " != " + std::to_string(expected));

    r.grid.resize(expected);
    for (std::size_t k = 0; k < expected; ++k) {
        const json::Value& cell = grid->items[k];
        if (!cell.IsNumber()) return MakeError("grid 원소가 숫자가 아님");
        const double v = cell.number;
        if (v < 0.0 || v > 65535.0) return MakeError("grid 원소가 타일 번호 범위 밖");
        r.grid[k] = static_cast<world::TileId>(v);
    }

    r.ok = true;
    return r;
}

// 타임아웃을 건 connect. 논블로킹으로 걸고 select 로 기다린다
// (블로킹 connect 는 서버가 없을 때 수십 초를 잡아먹는다).
net::SocketHandle ConnectWithTimeout(const std::string& host, uint16_t port, int timeoutMs) {
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* info = nullptr;
    const std::string portText = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portText.c_str(), &hints, &info) != 0 || info == nullptr)
        return net::kInvalidSocket;

    net::SocketHandle sock = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (sock == net::kInvalidSocket) {
        ::freeaddrinfo(info);
        return net::kInvalidSocket;
    }

    net::SetNonBlocking(sock, true);
    const int rc = ::connect(sock, info->ai_addr, static_cast<int>(info->ai_addrlen));
    ::freeaddrinfo(info);

    if (rc != 0) {
        if (!net::IsConnectPending(net::LastNetError())) {
            net::CloseSocket(sock);
            return net::kInvalidSocket;
        }
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(sock, &writable);
        timeval tv{};
        tv.tv_sec  = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        if (::select(static_cast<int>(sock) + 1, nullptr, &writable, nullptr, &tv) <= 0) {
            net::CloseSocket(sock);
            return net::kInvalidSocket;  // 시간 초과 = 서버가 없다
        }
        // select 가 깨어나도 연결이 성공했다는 뜻은 아니다. SO_ERROR 를 확인한다.
        int soErr = 0;
        socklen_t len = sizeof(soErr);
        if (::getsockopt(sock, SOL_SOCKET, SO_ERROR,
                         reinterpret_cast<char*>(&soErr), &len) != 0 || soErr != 0) {
            net::CloseSocket(sock);
            return net::kInvalidSocket;
        }
    }

    net::SetNonBlocking(sock, false);
    net::SetRecvTimeout(sock, kRecvTimeoutMs);

    int nodelay = 1;   // 작은 요청이 Nagle 로 지연되면 왕복 측정이 왜곡된다
    ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
    return sock;
}

} // namespace

RemoteShapeGenerator::~RemoteShapeGenerator() {
    Stop();
}

void RemoteShapeGenerator::Start(std::string host, uint16_t port) {
    if (m_running.load(std::memory_order_relaxed)) return;

    m_host = std::move(host);
    m_port = port;
    m_name = "remote " + m_host + ":" + std::to_string(m_port);

    m_stop.store(false, std::memory_order_relaxed);
    m_running.store(true, std::memory_order_relaxed);
    m_thread = std::thread(&RemoteShapeGenerator::Worker, this);
}

void RemoteShapeGenerator::Stop() {
    if (!m_running.exchange(false, std::memory_order_relaxed)) return;
    m_stop.store(true, std::memory_order_relaxed);
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
    m_connected.store(false, std::memory_order_relaxed);
}

void RemoteShapeGenerator::Request(const ShapeRequest& req) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // 대기 중이던 요청은 **버리고 덮어쓴다** — 마지막 것만 의미가 있다.
        m_job.req  = req;
        m_job.seq  = m_nextSeq++;
        m_latestSeq = m_job.seq;
        m_hasJob   = true;
        m_results.clear();   // 낡은 결과가 남아 있으면 새 요청의 응답으로 오해된다
    }
    m_cv.notify_one();
}

bool RemoteShapeGenerator::Poll(ShapeResult& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_results.empty()) {
        Done done = std::move(m_results.front());
        m_results.pop_front();
        if (done.seq != m_latestSeq) continue;   // 이미 밀려난 요청의 응답 — 버린다
        out = std::move(done.result);
        return true;
    }
    return false;
}

bool RemoteShapeGenerator::Pending() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hasJob || m_inFlight;
}

void RemoteShapeGenerator::Worker() {
    // WinSock 초기화는 프로세스당 참조 카운트라 NetClient 와 함께 떠 있어도 안전하다.
    if (!net::NetInit()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_results.push_back({MakeError("소켓 초기화 실패"), m_latestSeq});
        return;
    }

    net::SocketHandle sock = net::kInvalidSocket;
    std::vector<char> rxBuffer;

    auto dropConnection = [&]() {
        if (sock != net::kInvalidSocket) {
            net::CloseSocket(sock);
            sock = net::kInvalidSocket;
        }
        rxBuffer.clear();
        m_connected.store(false, std::memory_order_relaxed);
    };

    auto ensureConnected = [&]() {
        if (sock != net::kInvalidSocket) return true;
        sock = ConnectWithTimeout(m_host, m_port, kConnectTimeoutMs);
        m_connected.store(sock != net::kInvalidSocket, std::memory_order_relaxed);
        return sock != net::kInvalidSocket;
    };

    // 한 번의 송수신 시도. lostConnection=true 면 소켓이 죽은 것이라 재접속 후 재시도할 값어치가 있다.
    auto attempt = [&](const ShapeRequest& req, bool& lostConnection) -> ShapeResult {
        lostConnection = false;

        const std::string bodyText = BuildRequestJson(req);
        const std::vector<char> packet = net::BuildPacket(
            static_cast<net::Opcode>(AiOpcode::GenerateReq),
            bodyText.data(), static_cast<uint32_t>(bodyText.size()),
            nullptr, 0, kAiMaxBodySize);

        if (packet.empty()) return MakeError("요청이 너무 큽니다");
        if (!net::SendAll(sock, packet.data(), static_cast<int32_t>(packet.size()))) {
            dropConnection();
            lostConnection = true;
            return MakeError("요청 전송 실패 (연결이 끊겼습니다)");
        }

        // 응답 한 프레임을 기다린다. recv 타임아웃마다 깨어나 종료 요청을 확인한다.
        const auto deadline = Clock::now() + std::chrono::milliseconds(kReplyTimeoutMs);
        while (!m_stop.load(std::memory_order_relaxed)) {
            net::PacketHeader header{};
            std::vector<char> respBody;
            const net::FrameResult fr =
                net::TryExtractPacket(rxBuffer, header, respBody, kAiMaxBodySize);

            if (fr == net::FrameResult::Malformed) {
                dropConnection();
                lostConnection = true;
                return MakeError("응답 프레임이 규약을 벗어났습니다");
            }
            if (fr == net::FrameResult::Ok) {
                if (header.Opcode != static_cast<uint16_t>(AiOpcode::GenerateAck))
                    return MakeError("예상 밖 옵코드: " + std::to_string(header.Opcode));
                return ParseResponse(respBody, req);
            }
            if (Clock::now() >= deadline) {
                dropConnection();          // 스트림 위치를 믿을 수 없다
                lostConnection = true;
                return MakeError("응답 시간 초과");
            }

            char chunk[8192];
            const int received = ::recv(sock, chunk, static_cast<int>(sizeof(chunk)), 0);
            if (received > 0) {
                rxBuffer.insert(rxBuffer.end(), chunk, chunk + received);
            } else if (received == 0) {
                dropConnection();
                lostConnection = true;
                return MakeError("서버가 연결을 닫았습니다");
            } else if (!net::IsRecvTimeout(net::LastNetError())) {
                dropConnection();
                lostConnection = true;
                return MakeError("수신 오류 (연결이 끊겼습니다)");
            }
            // 타임아웃이면 한 바퀴 더 — 종료 플래그를 확인하려는 것이다.
        }
        return MakeError("종료 중 요청이 취소되었습니다");
    };

    ensureConnected();   // 기동 직후 한 번 붙어본다 — 패널이 처음부터 정확한 상태를 보여준다

    while (!m_stop.load(std::memory_order_relaxed)) {
        Job  job;
        bool haveJob = false;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            // 요청이 없어도 주기적으로 깨어난다. **서버를 나중에 켜도 패널이 살아나야** 하기 때문이다
            // (에디터를 먼저 열고 서버를 나중에 띄우는 순서가 오히려 흔하다).
            m_cv.wait_for(lock, std::chrono::milliseconds(kIdlePollMs),
                          [&] { return m_hasJob || m_stop.load(std::memory_order_relaxed); });
            if (m_stop.load(std::memory_order_relaxed)) break;
            if (m_hasJob) {
                job        = m_job;
                m_hasJob   = false;
                m_inFlight = true;
                haveJob    = true;
            }
        }

        if (!haveJob) {
            ensureConnected();   // 유휴 틱: 연결 상태만 갱신한다
            continue;
        }

        const auto started = Clock::now();
        ShapeResult result;

        if (!ensureConnected()) {
            result = MakeError("AI 서버에 연결할 수 없습니다 (" + m_host + ":" +
                               std::to_string(m_port) + ")");
        } else {
            bool lost = false;
            result = attempt(job.req, lost);

            // 유휴 상태로 오래 두면 서버가 먼저 연결을 닫는다(서버 타임아웃 300초).
            // 그때 첫 요청만 실패하고 사용자가 다시 눌러야 하는 건 버그처럼 보인다 —
            // 연결이 죽어서 실패한 경우에 한해 **한 번만** 재접속해서 다시 보낸다.
            if (lost && !m_stop.load(std::memory_order_relaxed) && ensureConnected()) {
                bool lostAgain = false;
                result = attempt(job.req, lostAgain);
            }
        }

        result.roundTripMs = MsSince(started);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_inFlight = false;
            m_results.push_back({std::move(result), job.seq});
        }
    }

    dropConnection();
    net::NetShutdown();
}

} // namespace gs::ai
