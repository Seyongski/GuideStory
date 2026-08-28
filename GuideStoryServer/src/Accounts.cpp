#include "Accounts.h"

#include "Crypto.h"
#include "net/Protocol.h"   // 길이 규칙 상수 — 클라와 같은 헤더를 본다
#include "sqlite3.h"

#include <cstdio>
#include <mutex>

namespace gs::server::accounts {

namespace {

sqlite3*   gDb = nullptr;
std::mutex gMutex;   // 여러 클라이언트 스레드가 동시에 로그인할 수 있다

bool Exec(const char* sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(gDb, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::printf("[계정] SQL 실패: %s\n", errMsg ? errMsg : "?");
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// 길이 규칙 검사. 프로토콜의 상수를 그대로 쓴다 — 클라이언트도 같은 값으로 미리 거른다.
// (고정 배열이 널 종료를 포함하므로 상한은 '미만' 이다.)
bool IsIdFormatValid(const std::string& loginId, const std::string& password) {
    if (loginId.size()  < net::kMinLoginIdLen  || loginId.size()  >= net::kMaxLoginIdLen)  return false;
    if (password.size() < net::kMinPasswordLen || password.size() >= net::kMaxPasswordLen) return false;
    return true;
}

bool IsNicknameValid(const std::string& nickname) {
    return nickname.size() >= net::kMinNicknameLen && nickname.size() < net::kMaxNameLen;
}

// 비밀번호를 솔트와 함께 해시한다.
void HashPassword(const std::string& password, const uint8_t* salt, uint8_t* outHash) {
    crypto::Pbkdf2HmacSha256(
        reinterpret_cast<const uint8_t*>(password.data()), password.size(),
        salt, crypto::kSaltSize,
        crypto::kPbkdf2Iterations, outHash);
}

// 특정 컬럼에 그 값이 이미 있는가. 가입 실패 사유를 아이디/닉네임으로 갈라 알려주기 위한 것이다
// (UNIQUE 위반만 보면 둘 중 어느 쪽인지 알 수 없다). gMutex 를 잡은 상태로 호출한다.
bool ColumnHasValue(const char* sql, const std::string& value) {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(gDb, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    const bool found = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    return found;
}

} // namespace

bool Start(const char* dbPath) {
    if (gDb != nullptr) return true;

    if (sqlite3_open(dbPath, &gDb) != SQLITE_OK) {
        std::printf("[계정] DB 열기 실패: %s\n", sqlite3_errmsg(gDb));
        sqlite3_close(gDb);
        gDb = nullptr;
        return false;
    }

    // 채팅 로그와 같은 파일을 공유할 수 있으므로 여기서도 WAL 로 맞춘다.
    // 계정은 유실되면 안 되므로 synchronous 는 FULL 로 둔다(로그와 다른 점).
    Exec("PRAGMA journal_mode=WAL;");
    Exec("PRAGMA synchronous=FULL;");
    // 다른 커넥션이 쓰는 중이면 즉시 실패하지 말고 5초까지 기다린다.
    sqlite3_busy_timeout(gDb, 5000);

    const char* kSchema =
        "CREATE TABLE IF NOT EXISTS accounts("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"          // 이게 곧 UserId 다
        "  login_id   TEXT    NOT NULL UNIQUE COLLATE NOCASE,"     // 대소문자 구분 없이 유일
        "  pw_salt    TEXT    NOT NULL,"                           // 16진 문자열
        "  pw_hash    TEXT    NOT NULL,"                           // 16진 문자열. 평문 비번은 어디에도 없다
        "  nickname   TEXT    NOT NULL UNIQUE COLLATE NOCASE,"     // 귓속말 대상 지정에 쓰므로 유일해야 한다
        "  created_at INTEGER NOT NULL"
        ");";

    if (!Exec(kSchema)) {
        sqlite3_close(gDb);
        gDb = nullptr;
        return false;
    }

    std::printf("[계정] %s 준비 완료\n", dbPath);
    return true;
}

void Stop() {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gDb != nullptr) {
        sqlite3_close(gDb);
        gDb = nullptr;
    }
}

AccountResult Create(const std::string& loginId, const std::string& password,
                     const std::string& nickname, uint64_t& outUserId) {
    if (!IsIdFormatValid(loginId, password) || !IsNicknameValid(nickname)) {
        return AccountResult::InvalidFormat;
    }

    std::lock_guard<std::mutex> lock(gMutex);
    if (gDb == nullptr) return AccountResult::DbError;

    // 어느 쪽이 겹쳤는지 먼저 확인한다. INSERT 의 UNIQUE 위반만 보면 구분할 수 없고,
    // 사용자에게 "이미 있는 아이디" 와 "이미 있는 닉네임" 은 다음 행동이 다른 정보다.
    if (ColumnHasValue("SELECT 1 FROM accounts WHERE login_id = ?;", loginId)) {
        return AccountResult::DuplicateId;
    }
    if (ColumnHasValue("SELECT 1 FROM accounts WHERE nickname = ?;", nickname)) {
        return AccountResult::DuplicateNick;
    }

    uint8_t salt[crypto::kSaltSize];
    crypto::RandomBytes(salt, sizeof(salt));

    uint8_t hash[crypto::kSha256Size];
    HashPassword(password, salt, hash);

    const std::string saltHex = crypto::ToHex(salt, sizeof(salt));
    const std::string hashHex = crypto::ToHex(hash, sizeof(hash));

    sqlite3_stmt* st = nullptr;
    const char* sql =
        "INSERT INTO accounts(login_id, pw_salt, pw_hash, nickname, created_at)"
        " VALUES(?,?,?,?, strftime('%s','now'));";

    if (sqlite3_prepare_v2(gDb, sql, -1, &st, nullptr) != SQLITE_OK) {
        std::printf("[계정] INSERT 준비 실패: %s\n", sqlite3_errmsg(gDb));
        return AccountResult::DbError;
    }

    sqlite3_bind_text(st, 1, loginId.c_str(),  static_cast<int>(loginId.size()),  SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, saltHex.c_str(),  static_cast<int>(saltHex.size()),  SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, hashHex.c_str(),  static_cast<int>(hashHex.size()),  SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, nickname.c_str(), static_cast<int>(nickname.size()), SQLITE_TRANSIENT);

    const int step = sqlite3_step(st);
    sqlite3_finalize(st);

    if (step != SQLITE_DONE) {
        // 위 사전 검사와 INSERT 사이에 다른 프로세스가 같은 값을 넣었을 때 여기로 온다.
        // 어느 컬럼인지는 알 수 없으므로 아이디 중복으로 뭉뚱그린다(드문 경합).
        if (sqlite3_extended_errcode(gDb) == SQLITE_CONSTRAINT_UNIQUE) {
            return AccountResult::DuplicateId;
        }
        std::printf("[계정] INSERT 실패: %s\n", sqlite3_errmsg(gDb));
        return AccountResult::DbError;
    }

    outUserId = static_cast<uint64_t>(sqlite3_last_insert_rowid(gDb));
    return AccountResult::Success;
}

AccountResult Authenticate(const std::string& loginId, const std::string& password,
                           uint64_t& outUserId, std::string& outNickname) {
    if (!IsIdFormatValid(loginId, password)) return AccountResult::InvalidFormat;

    std::lock_guard<std::mutex> lock(gMutex);
    if (gDb == nullptr) return AccountResult::DbError;

    sqlite3_stmt* st = nullptr;
    const char* sql = "SELECT id, pw_salt, pw_hash, nickname FROM accounts WHERE login_id = ?;";
    if (sqlite3_prepare_v2(gDb, sql, -1, &st, nullptr) != SQLITE_OK) {
        std::printf("[계정] SELECT 준비 실패: %s\n", sqlite3_errmsg(gDb));
        return AccountResult::DbError;
    }
    sqlite3_bind_text(st, 1, loginId.c_str(), static_cast<int>(loginId.size()), SQLITE_TRANSIENT);

    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        return AccountResult::NotFound;
    }

    const uint64_t id       = static_cast<uint64_t>(sqlite3_column_int64(st, 0));
    const char*    saltText = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
    const char*    hashText = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
    const char*    nickText = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));

    const std::string saltHex  = saltText ? saltText : "";
    const std::string hashHex  = hashText ? hashText : "";
    const std::string nickname = nickText ? nickText : "";
    sqlite3_finalize(st);

    uint8_t salt[crypto::kSaltSize];
    uint8_t stored[crypto::kSha256Size];
    if (!crypto::FromHex(saltHex, salt, sizeof(salt)) ||
        !crypto::FromHex(hashHex, stored, sizeof(stored))) {
        std::printf("[계정] 저장된 해시 형식이 깨졌다: login_id=%s\n", loginId.c_str());
        return AccountResult::DbError;
    }

    uint8_t computed[crypto::kSha256Size];
    HashPassword(password, salt, computed);

    // memcmp 대신 상수 시간 비교. 타이밍으로 정답을 좁혀가는 공격을 막는다.
    if (!crypto::ConstantTimeEquals(computed, stored, crypto::kSha256Size)) {
        return AccountResult::WrongPassword;
    }

    outUserId   = id;
    outNickname = nickname;
    return AccountResult::Success;
}

} // namespace gs::server::accounts
