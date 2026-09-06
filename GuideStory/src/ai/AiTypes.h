#pragma once

#include "world/TileMap.h" // TileId, kEmptyTile

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// AI 도형 생성의 요청/응답 타입 — 생성기 구현(소켓/libtorch)에 의존하지 않는 순수 데이터.
// SDL 비의존이며 엔진 바깥 라이브러리도 쓰지 않는다(ADR-006과 같은 격리 원칙).
// 설계: docs/ai-shape-synthesis.md, docs/ai-roadmap.md
namespace gs::ai {

// 생성 가능한 도형 라벨 — **닫힌 집합**(ADR-012).
// 자유 텍스트를 받지 않는 이유: 채점 가능한 지표(인식률/IoU)를 정의하려면 정답 집합이
// 유한해야 한다. 미등록 단어는 추측하지 않고 거절한다.
//
// 열거자 순서 = 학습 모델의 라벨 인덱스다. **항상 뒤에만 추가한다**
// (net/Protocol.h 옵코드와 같은 규칙 — 중간에 끼우면 학습된 체크포인트의
//  라벨 임베딩이 통째로 어긋나고, 그 사고는 런타임까지 조용히 간다).
enum class ShapeLabel : int {
    Square = 0,
    Circle,
    Triangle,
    Cross,
    Star,
    Heart,
    Count
};

inline constexpr int kShapeLabelCount = static_cast<int>(ShapeLabel::Count);

struct ShapeLabelInfo {
    ShapeLabel  label;
    const char* key;         // 프로토콜 JSON·맵 파일 CONCEPT에 쓰는 정규 키(ASCII, 공백 없음)
    const char* name;        // 에디터 UI 표시 이름
    const char* synonyms[3]; // 추가 동의어(널 종료). key/name은 자동 인식하므로 넣지 않는다
};

// key는 파일과 프로토콜에 박히는 값이다 — 한 번 정하면 바꾸지 않는다(기존 맵의 CONCEPT가 깨진다).
inline constexpr ShapeLabelInfo kShapeLabels[] = {
    {ShapeLabel::Square,   "square",   "사각형", {"네모",     "정사각형", nullptr}},
    {ShapeLabel::Circle,   "circle",   "원",     {"동그라미", "원형",     nullptr}},
    {ShapeLabel::Triangle, "triangle", "삼각형", {"세모",     nullptr,    nullptr}},
    {ShapeLabel::Cross,    "cross",    "십자",   {"크로스",   "더하기",   nullptr}},
    {ShapeLabel::Star,     "star",     "별",     {"스타",     "별표",     nullptr}},
    {ShapeLabel::Heart,    "heart",    "하트",   {"심장",     nullptr,    nullptr}},
};
static_assert(std::size(kShapeLabels) == static_cast<std::size_t>(kShapeLabelCount),
              "kShapeLabels가 ShapeLabel과 어긋났다 — 라벨을 추가하면 표도 함께 채운다");

inline const ShapeLabelInfo& ShapeLabelAt(ShapeLabel label) {
    const int i = static_cast<int>(label);
    return kShapeLabels[(i < 0 || i >= kShapeLabelCount) ? 0 : i];
}

inline const char* ShapeLabelKey(ShapeLabel label)  { return ShapeLabelAt(label).key; }
inline const char* ShapeLabelName(ShapeLabel label) { return ShapeLabelAt(label).name; }

namespace detail {
// 앞뒤 공백 제거 + ASCII 소문자화(한글 바이트는 그대로 둔다 — UTF-8 멀티바이트는 0x80 이상이라
// std::tolower가 건드리지 않게 unsigned char로 승격해서 넘긴다).
inline std::string NormalizeLabelText(std::string_view text) {
    std::size_t b = 0, e = text.size();
    while (b < e && static_cast<unsigned char>(text[b]) <= ' ') ++b;
    while (e > b && static_cast<unsigned char>(text[e - 1]) <= ' ') --e;

    std::string out;
    out.reserve(e - b);
    for (std::size_t i = b; i < e; ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        out.push_back(c < 0x80 ? static_cast<char>(std::tolower(c)) : text[i]);
    }
    return out;
}
} // namespace detail

// 사용자 입력 → 라벨. 정규 키·UI 이름·동의어를 모두 받는다("heart"/"하트"/"심장").
// **모르는 단어는 추측하지 않고 false를 돌려준다**(ADR-012). 가까운 라벨로 임의 매핑하면
// 사용자는 자기가 뭘 요청했는지 모른 채 엉뚱한 결과를 받는다.
inline bool ParseShapeLabel(std::string_view text, ShapeLabel& out) {
    const std::string q = detail::NormalizeLabelText(text);
    if (q.empty()) return false;

    for (const ShapeLabelInfo& info : kShapeLabels) {
        if (q == info.key || q == info.name) {
            out = info.label;
            return true;
        }
        for (const char* syn : info.synonyms) {
            if (syn == nullptr) break;
            if (q == syn) {
                out = info.label;
                return true;
            }
        }
    }
    return false;
}

// 생성 요청. 에디터가 채워 생성기에 넘긴다.
struct ShapeRequest {
    ShapeLabel    label = ShapeLabel::Circle;
    int           w     = 32;  // 생성 격자 폭(칸)
    int           h     = 32;  // 생성 격자 높이(칸)
    world::TileId tile  = 1;   // 채울 타일 번호(core::TilePalette). v1은 사용자가 고른다
    unsigned int  seed  = 0;   // 0 = 생성기가 임의로 정한다(재생성마다 다른 결과)
};

// 생성 결과. **실패도 결과다** — 예외를 던지지 않고 ok=false + error로 돌아온다.
// AI는 부가 기능이므로 실패가 편집 흐름을 끊으면 안 된다.
struct ShapeResult {
    bool                       ok = false;
    int                        w  = 0;
    int                        h  = 0;
    std::vector<world::TileId> grid;  // w*h개, row-major. 0 = 빈 칸
    std::string                error; // ok=false일 때 사유(에디터가 그대로 표시)

    // 재현성: 어떤 모델이 어떤 seed로 만들었는지. 맵 헤더 AIGEN에 기록해 같은 결과를 다시 뽑는다.
    std::string  model;      // "stub-raster" / "cvae_v1" / ...
    unsigned int seed = 0;   // 실제 사용된 seed(요청이 0이었으면 생성기가 정한 값)

    // ADR-007 증명 과제의 측정값. 두 값의 차이가 곧 IPC 오버헤드다
    // (인프로세스 libtorch 구현에서는 차이가 거의 0이 되어야 한다).
    double roundTripMs = 0.0; // 요청 → 수신, 클라이언트 측정
    double inferenceMs = 0.0; // 생성기 내부 추론 시간, 생성기가 보고

    world::TileId At(int x, int y) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return world::kEmptyTile;
        return grid[static_cast<std::size_t>(y) * w + x];
    }

    // 격자 크기와 버퍼 길이가 맞는지. 응답은 **프로세스 밖에서 오므로** 쓰기 전에 검사한다
    // (net/Framing이 선언된 길이를 신뢰하기 전에 상한을 검사하는 것과 같은 이유).
    bool Valid() const {
        return ok && w > 0 && h > 0 &&
               grid.size() == static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    }
};

} // namespace gs::ai
