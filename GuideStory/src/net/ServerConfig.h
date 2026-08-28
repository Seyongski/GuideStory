#pragma once

#include <cstdint>
#include <fstream>
#include <string>

// 접속할 계정/채팅 서버 주소. assets/config/server.txt 에서 읽는다.
//
// [왜 파일인가]
//   빌드 없이 바꿀 수 있어야 한다 — 개발 중에는 127.0.0.1, 팀원끼리 붙을 때는 상대 PC,
//   나중에는 데디케이트 서버 주소(ADR-001)로 바뀐다. 코드 상수로 두면 그때마다 다시 빌드해야 한다.
//   PlayerState / keybindings 와 같은 자리(assets/config), 같은 경량 텍스트 포맷을 쓴다.
//
// 파일이 없으면 기본값(127.0.0.1:7777)을 그대로 쓴다. 서버를 안 띄우고 게임만 돌려보는
// 경우가 흔하므로, 파일이 없는 것은 오류가 아니다.
//
// net/ 은 platform/ 을 모른다(ADR-006 계층 규칙). 경로 해석(AssetsDir)은 호출측이 하고
// 여기는 완성된 경로만 받는다.
namespace gs::net {

// 경량 텍스트 포맷:
//   GSSERVER 1
//   HOST 127.0.0.1
//   PORT 7777
class ServerConfig {
public:
    void Load(const std::string& path) {
        std::ifstream in(path);
        if (!in) return;                       // 없으면 기본값 유지

        std::string tag;
        int version = 0;
        if (!(in >> tag >> version) || tag != "GSSERVER") return;   // 형식이 다르면 기본값 유지

        std::string key;
        while (in >> key) {
            if (key == "HOST") {
                in >> m_host;
            } else if (key == "PORT") {
                int port = 0;
                in >> port;
                // 0 이나 범위 밖이면 무시한다. 오타 하나로 접속이 조용히 실패하는 것보다
                // 기본 포트로 붙어보고 실패 사유를 화면에 보여주는 편이 진단이 빠르다.
                if (port > 0 && port <= 65535) m_port = static_cast<uint16_t>(port);
            }
        }
    }

    const std::string& Host() const { return m_host; }
    uint16_t           Port() const { return m_port; }

private:
    std::string m_host = "127.0.0.1";
    uint16_t    m_port = 7777;
};

} // namespace gs::net
