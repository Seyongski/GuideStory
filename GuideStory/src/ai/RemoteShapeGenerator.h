#pragma once

#include "ai/AiProtocol.h"
#include "ai/IShapeGenerator.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

// 파이썬 추론 서버(GuideStoryAI/serve/ai_server.py)에 소켓으로 질의하는 생성기.
//
// [스레드 경계 — net::NetClient 와 같은 모델]
//
//     에디터 스레드                        워커 스레드
//   ┌──────────────────────┐            ┌────────────────────────────┐
//   │ MapEditorScreen      │            │  RemoteShapeGenerator      │
//   │  Request()  ─────────┼─ 요청 슬롯 ▶│   connect / send / recv    │
//   │  Poll()     ◀────────┼─ 결과 큐 ──│   (블로킹 호출은 전부 여기) │
//   └──────────────────────┘            └────────────────────────────┘
//
//   >> 워커는 IRenderDevice / Map / Screen 을 절대 건드리지 않는다. <<
//   순수 데이터(ShapeResult)만 큐에 넣고, 에디터 스레드가 Poll() 로 꺼내 그때 화면을 바꾼다.
//
// [요청 슬롯이 큐가 아닌 이유]
//   재생성(R)을 연타하면 큐에는 낡은 요청이 쌓인다. 사용자가 원하는 건 **마지막 것 하나**다.
//   그래서 대기 중인 요청은 새 요청이 덮어쓰고, 이미 날아간 요청의 응답은 seq 로 걸러 버린다.
//
// [소켓 타입이 여기 없는 이유]
//   net/Socket.h 는 winsock2.h 를 끌고 오는데 그게 SDL 을 쓰는 TU 에 섞이면 재정의 오류가
//   난다(net/NetClient.h 와 같은 이유). 소켓은 .cpp 안 워커 함수의 지역 변수로만 존재한다.
namespace gs::ai {

class RemoteShapeGenerator final : public IShapeGenerator {
public:
    RemoteShapeGenerator() = default;
    ~RemoteShapeGenerator() override;

    RemoteShapeGenerator(const RemoteShapeGenerator&)            = delete;
    RemoteShapeGenerator& operator=(const RemoteShapeGenerator&) = delete;

    // 워커 스레드를 띄운다. 접속은 워커가 첫 요청 때 하고, 끊기면 다음 요청에서 다시 붙는다.
    // **서버가 꺼져 있어도 실패하지 않는다** — 요청이 실패 결과로 돌아올 뿐 에디터는 정상이다.
    void Start(std::string host, uint16_t port = kDefaultAiPort);
    void Stop();

    // --- IShapeGenerator ---
    bool        Available() const override { return m_running.load(std::memory_order_relaxed); }
    bool        Ready()     const override { return m_connected.load(std::memory_order_relaxed); }
    const char* Name()      const override { return m_name.c_str(); }
    void        Request(const ShapeRequest& req) override;
    bool        Poll(ShapeResult& out) override;
    bool        Pending() const override;

    // 진단용. 화면에 "연결됨/끊김"을 보여주면 실패 원인을 훨씬 빨리 좁힌다.
    bool               Connected() const { return m_connected.load(std::memory_order_relaxed); }
    const std::string& Host() const { return m_host; }
    uint16_t           Port() const { return m_port; }

private:
    void Worker();

    struct Job {
        ShapeRequest  req;
        std::uint64_t seq = 0;
    };
    struct Done {
        ShapeResult   result;
        std::uint64_t seq = 0;
    };

    // --- Start 이후 불변 ---
    std::string m_host;
    uint16_t    m_port = kDefaultAiPort;
    std::string m_name = "none";   // "remote 127.0.0.1:7788" — Name() 이 돌려준다

    std::thread             m_thread;
    std::atomic<bool>       m_stop{false};
    std::atomic<bool>       m_running{false};
    std::atomic<bool>       m_connected{false};

    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;

    bool                    m_hasJob = false;   // 대기 중인 요청 슬롯(큐가 아니다)
    Job                     m_job;
    std::uint64_t           m_nextSeq = 1;
    std::uint64_t           m_latestSeq = 0;    // 가장 최근에 요청한 seq — 이것만 유효하다
    bool                    m_inFlight = false; // 워커가 처리 중
    std::deque<Done>        m_results;
};

} // namespace gs::ai
