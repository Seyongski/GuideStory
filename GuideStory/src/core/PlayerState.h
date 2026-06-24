#pragma once

#include <fstream>
#include <string>

// 로컬 플레이어 상태(단일 플레이). 멀티플레이(3단계, ADR-001)의 서버측 플레이어 레코드의 전신이다.
// 핵심 원칙: 정적 맵 콘텐츠(.gsmap)와 동적 플레이어 상태를 분리한다 — 맵 파일에 플레이어 상태를 넣지 않는다.
// 지금은 "마지막으로 있던 맵"만 기억한다(추후 위치/레벨/인벤토리 등으로 확장).
namespace gs::core {

// 경량 텍스트 포맷:
//   GSPLAYER 1
//   LASTMAP <맵 파일명>      (assets/maps 기준 파일명; 없으면 줄 생략)
class PlayerState {
public:
    // 저장 경로를 기억하고, 파일이 있으면 읽어 lastMap을 채운다(없거나 형식 오류면 비운 채 유지).
    void Load(const std::string& path) {
        m_path = path;
        std::ifstream in(path);
        if (!in) return;
        std::string tag;
        int version = 0;
        if (!(in >> tag >> version) || tag != "GSPLAYER") return;
        std::string key;
        while (in >> key) {
            if (key == "LASTMAP") in >> m_lastMap;
        }
    }

    // 기억해 둔 경로에 기록한다. 경로가 없으면(=Load 전) 아무것도 하지 않는다.
    void Save() const {
        if (m_path.empty()) return;
        std::ofstream out(m_path, std::ios::trunc);
        if (!out) return;
        out << "GSPLAYER 1\n";
        if (!m_lastMap.empty()) out << "LASTMAP " << m_lastMap << "\n";
    }

    const std::string& LastMap() const { return m_lastMap; }

    // 마지막 맵을 갱신하고 즉시 저장한다(맵 진입·전환 시 호출). 매 전환마다 파일에 남기므로
    // ESC·창닫기·비정상 종료 어느 쪽이든 "마지막으로 있던 맵"이 보존된다. 값이 같으면 재기록 생략.
    void SetLastMap(const std::string& mapFile) {
        if (mapFile.empty() || mapFile == m_lastMap) return;
        m_lastMap = mapFile;
        Save();
    }

private:
    std::string m_path;     // Load로 받은 저장 경로(Save가 재사용)
    std::string m_lastMap;  // 마지막으로 진입한 맵 파일명
};

} // namespace gs::core
