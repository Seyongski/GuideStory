#include "Session.h"

#include <algorithm>

namespace gs::server {

SessionPtr SessionManager::Add(net::SocketHandle sock) {
    SessionPtr session = std::make_shared<ClientSession>();
    session->sock = sock;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.push_back(session);
    return session;
}

void SessionManager::Remove(const SessionPtr& session) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto it = std::find(m_sessions.begin(), m_sessions.end(), session);
    if (it != m_sessions.end()) m_sessions.erase(it);

    // 목록에서 빠진 뒤에 닫는다. 락을 잡은 채로 처리하므로
    // 다른 스레드가 ForEach 로 순회 중인 소켓을 닫아버리는 일은 없다.
    if (session->sock != net::kInvalidSocket) {
        net::CloseSocket(session->sock);
        session->sock = net::kInvalidSocket;
    }
}

std::size_t SessionManager::Count() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessions.size();
}

bool SessionManager::IsUserOnline(uint64_t userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const SessionPtr& session : m_sessions) {
        if (session->authed && session->userId == userId) return true;
    }
    return false;
}

} // namespace gs::server
