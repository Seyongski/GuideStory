#include "ChatLog.h"

#include "sqlite3.h"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace gs::server::chatlog {

namespace {

// 큐 상한. 디스크가 느려도 메모리가 무한정 늘지 않게 막는다.
// 한 줄이 대략 200바이트 안팎이므로 1만 줄이면 몇 MB 수준이다.
constexpr std::size_t kMaxQueuedEntries = 10000;

// 한 트랜잭션에 몰아 쓸 최대 줄 수.
// 줄마다 커밋하면 커밋마다 디스크 동기화가 일어나 훨씬 느리다.
constexpr std::size_t kMaxBatchRows = 256;

struct LogEntry {
    int64_t     timestamp    = 0;
    uint64_t    senderUserId = 0;
    std::string senderName;
    std::string targetName;   // 귓속말 수신자. 그 외 채널은 빈 문자열
    std::string text;
    uint8_t     channel = 0;
};

std::mutex              gMutex;
std::condition_variable gCv;
std::deque<LogEntry>    gQueue;
bool                    gStopping = false;

std::thread   gWriterThread;
sqlite3*      gDb = nullptr;
sqlite3_stmt* gInsertStmt = nullptr;

std::atomic<uint64_t> gWritten{0};
std::atomic<uint64_t> gDropped{0};

// 큐가 넘칠 때 로그를 매번 찍으면 콘솔이 도배된다. 처음 한 번만 알린다.
bool gWarnedOverflow = false;

bool Exec(const char* sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(gDb, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::printf("[채팅로그] SQL 실패: %s (%s)\n", errMsg ? errMsg : "?", sql);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// 한 묶음을 트랜잭션 하나로 쓴다.
// 이 함수는 라이터 스레드에서만 불리므로 gDb / gInsertStmt 에 락이 필요 없다.
void WriteBatch(const std::vector<LogEntry>& batch) {
    if (batch.empty() || gDb == nullptr) return;
    if (!Exec("BEGIN;")) return;

    uint64_t okCount = 0;
    for (const LogEntry& e : batch) {
        sqlite3_reset(gInsertStmt);
        sqlite3_clear_bindings(gInsertStmt);

        sqlite3_bind_int64(gInsertStmt, 1, e.timestamp);
        sqlite3_bind_int64(gInsertStmt, 2, static_cast<sqlite3_int64>(e.senderUserId));
        // SQLITE_TRANSIENT: sqlite 가 문자열을 자체 복사하게 한다.
        // STATIC 을 쓰면 batch 가 사라진 뒤를 가리키게 된다.
        sqlite3_bind_text(gInsertStmt, 3, e.senderName.c_str(),
                          static_cast<int>(e.senderName.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int(gInsertStmt, 4, e.channel);
        sqlite3_bind_text(gInsertStmt, 5, e.targetName.c_str(),
                          static_cast<int>(e.targetName.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(gInsertStmt, 6, e.text.c_str(),
                          static_cast<int>(e.text.size()), SQLITE_TRANSIENT);

        if (sqlite3_step(gInsertStmt) == SQLITE_DONE) {
            ++okCount;
        } else {
            std::printf("[채팅로그] INSERT 실패: %s\n", sqlite3_errmsg(gDb));
        }
    }
    sqlite3_reset(gInsertStmt);

    if (Exec("COMMIT;")) {
        gWritten.fetch_add(okCount, std::memory_order_relaxed);
    } else {
        Exec("ROLLBACK;");
    }
}

// DB 라이터 스레드의 본체. 서버 전체에 이 스레드 하나뿐이다.
void WriterLoop() {
    std::vector<LogEntry> batch;
    batch.reserve(kMaxBatchRows);

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(gMutex);

            // 할 일이 없으면 잔다. 폴링하지 않으므로 유휴 시 CPU 를 쓰지 않는다.
            gCv.wait(lock, [] { return gStopping || !gQueue.empty(); });

            // 종료 신호가 와도 큐가 남아있으면 마저 쓴다(정상 종료 시 무손실).
            if (gQueue.empty()) {
                if (gStopping) break;
                continue;
            }

            const std::size_t take = (gQueue.size() < kMaxBatchRows) ? gQueue.size() : kMaxBatchRows;
            for (std::size_t i = 0; i < take; ++i) {
                batch.push_back(std::move(gQueue.front()));
                gQueue.pop_front();
            }
        }
        // 락을 놓고 쓴다. 디스크 I/O 동안 클라이언트 스레드가 Enqueue 를 계속할 수 있다.
        WriteBatch(batch);
        batch.clear();
    }
}

} // namespace

bool Start(const char* dbPath) {
    if (gDb != nullptr) return true;   // 이미 시작됨

    if (sqlite3_open(dbPath, &gDb) != SQLITE_OK) {
        std::printf("[채팅로그] DB 열기 실패: %s\n", sqlite3_errmsg(gDb));
        sqlite3_close(gDb);
        gDb = nullptr;
        return false;
    }

    // WAL: 읽는 쪽(나중에 검색/신고 조회)이 쓰는 쪽을 막지 않는다.
    // synchronous=NORMAL: WAL 에서는 커밋마다 fsync 하지 않는다.
    //   OS 가 죽으면 최근 몇 커밋이 날아갈 수 있지만 DB 가 깨지지는 않는다.
    //   채팅 로그에는 충분한 보증이고, FULL(계정이 쓰는 값)보다 훨씬 빠르다.
    Exec("PRAGMA journal_mode=WAL;");
    Exec("PRAGMA synchronous=NORMAL;");

    const char* kSchema =
        "CREATE TABLE IF NOT EXISTS chat_log("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  ts          INTEGER NOT NULL,"   // Unix epoch(초). 패킷의 Timestamp 그대로
        "  sender_id   INTEGER NOT NULL,"   // accounts.id
        "  sender_name TEXT    NOT NULL,"
        "  channel     INTEGER NOT NULL,"   // net::ChatChannel
        "  target_name TEXT    NOT NULL,"   // 귓속말 수신자(그 외에는 빈 문자열)
        "  text        TEXT    NOT NULL"
        ");"
        // "최근 N줄" 조회가 주 용도라 시간 인덱스를 둔다.
        "CREATE INDEX IF NOT EXISTS idx_chat_log_ts ON chat_log(ts);";

    if (!Exec(kSchema)) {
        sqlite3_close(gDb);
        gDb = nullptr;
        return false;
    }

    // 구문을 매번 파싱하지 않도록 한 번만 준비해두고 재사용한다.
    const char* kInsert =
        "INSERT INTO chat_log(ts, sender_id, sender_name, channel, target_name, text)"
        " VALUES(?,?,?,?,?,?);";

    if (sqlite3_prepare_v2(gDb, kInsert, -1, &gInsertStmt, nullptr) != SQLITE_OK) {
        std::printf("[채팅로그] INSERT 준비 실패: %s\n", sqlite3_errmsg(gDb));
        sqlite3_close(gDb);
        gDb = nullptr;
        return false;
    }

    gStopping = false;
    gWriterThread = std::thread(WriterLoop);

    std::printf("[채팅로그] %s 에 기록한다 (SQLite %s)\n", dbPath, sqlite3_libversion());
    return true;
}

void Enqueue(int64_t timestamp, uint64_t senderUserId, const std::string& senderName,
             uint8_t channel, const std::string& targetName,
             const char* text, uint16_t textLen) {
    if (gDb == nullptr) {
        // Start 가 실패한 상태. 채팅은 계속되어야 하므로 조용히 버리되 집계는 한다.
        gDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    LogEntry entry;
    entry.timestamp    = timestamp;
    entry.senderUserId = senderUserId;
    entry.senderName   = senderName;
    entry.targetName   = targetName;
    entry.channel      = channel;
    entry.text.assign(text, textLen);   // 널 종료가 아니므로 길이로 복사한다

    {
        std::lock_guard<std::mutex> lock(gMutex);

        if (gQueue.size() >= kMaxQueuedEntries) {
            gDropped.fetch_add(1, std::memory_order_relaxed);
            if (!gWarnedOverflow) {
                gWarnedOverflow = true;
                std::printf("[채팅로그] 큐가 상한(%zu)에 도달해 기록을 버리기 시작한다."
                            " 디스크가 채팅 속도를 못 따라가고 있다.\n", kMaxQueuedEntries);
            }
            return;
        }

        gQueue.push_back(std::move(entry));
    }
    // 락 밖에서 깨운다. 락을 쥔 채 깨우면 깨어난 스레드가 곧바로 락 대기에 걸린다.
    gCv.notify_one();
}

void Stop() {
    if (gWriterThread.joinable()) {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gStopping = true;
        }
        gCv.notify_one();
        gWriterThread.join();   // 남은 큐를 다 쓸 때까지 기다린다
    }

    if (gInsertStmt != nullptr) {
        sqlite3_finalize(gInsertStmt);
        gInsertStmt = nullptr;
    }
    if (gDb != nullptr) {
        sqlite3_close(gDb);
        gDb = nullptr;
        std::printf("[채팅로그] 종료. 기록 %llu줄, 유실 %llu줄\n",
                    static_cast<unsigned long long>(gWritten.load()),
                    static_cast<unsigned long long>(gDropped.load()));
    }
}

uint64_t WrittenCount() { return gWritten.load(std::memory_order_relaxed); }
uint64_t DroppedCount() { return gDropped.load(std::memory_order_relaxed); }

} // namespace gs::server::chatlog
