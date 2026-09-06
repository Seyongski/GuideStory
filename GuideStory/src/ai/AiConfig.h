#pragma once

#include "ai/AiProtocol.h"

#include <cstdint>
#include <fstream>
#include <string>

// 접속할 AI 추론 서버 주소. assets/config/ai.txt 에서 읽는다.
// net::ServerConfig 와 같은 자리, 같은 경량 텍스트 포맷이다 — 두 서버를 다르게 다룰 이유가 없다.
//
// 파일이 없으면 기본값(127.0.0.1:7788)을 쓴다. **없는 것은 오류가 아니다** —
// AI 서버를 안 띄우고 에디터만 여는 경우가 오히려 흔하다.
//
// ai/ 는 platform/ 을 모른다(ADR-006 계층 규칙). 경로 해석(AssetsDir)은 호출측이 하고
// 여기는 완성된 경로만 받는다.
namespace gs::ai {

// 경량 텍스트 포맷:
//   GSAI 1
//   HOST 127.0.0.1
//   PORT 7788
//   ENABLED 1
class AiConfig {
public:
    void Load(const std::string& path) {
        std::ifstream in(path);
        if (!in) return;                    // 없으면 기본값 유지

        std::string tag;
        int version = 0;
        if (!(in >> tag >> version) || tag != "GSAI") return;   // 형식이 다르면 기본값 유지

        std::string key;
        while (in >> key) {
            if (key == "HOST") {
                in >> m_host;
            } else if (key == "PORT") {
                int port = 0;
                in >> port;
                // 오타 하나로 조용히 실패하는 것보다, 기본 포트로 붙어보고 사유를 보여주는 편이 낫다.
                if (port > 0 && port <= 65535) m_port = static_cast<uint16_t>(port);
            } else if (key == "ENABLED") {
                int on = 1;
                in >> on;
                m_enabled = (on != 0);
            }
        }
    }

    // false 면 에디터는 NullShapeGenerator 를 쓴다. 서버를 띄워두고도 AI를 끄고
    // 작업하고 싶을 때가 있다 — 그때 포트를 바꾸는 것보다 이 줄 하나가 명확하다.
    bool               Enabled() const { return m_enabled; }
    const std::string& Host()    const { return m_host; }
    uint16_t           Port()    const { return m_port; }

private:
    std::string m_host    = "127.0.0.1";
    uint16_t    m_port    = kDefaultAiPort;
    bool        m_enabled = true;
};

} // namespace gs::ai
