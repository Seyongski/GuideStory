#include "editor/AiShapeTool.h"

#include "ai/AiProtocol.h"
#include "ai/ShapeToMap.h"
#include "core/TilePalette.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace gs::editor {

namespace {

constexpr platform::Color kSelect  {120, 220, 255, 235}; // 선택 사각형
constexpr platform::Color kFhGhost { 90, 255, 160, 230}; // 예상 풋홀드
constexpr platform::Color kHint    {230, 236, 250, 255};

// 고스트 타일은 원래 타일 색을 반투명으로 — "이 색으로 놓인다"가 바로 보여야 한다.
platform::Color GhostColor(world::TileId id) {
    platform::Color c = core::TileColor(id);
    c.a = 130;
    return c;
}

// seed 0은 "서버가 정한다"는 뜻이므로 재생성마다 겹치지 않는 값을 만든다.
unsigned int NextSeed() {
    static unsigned int counter = 0;
    ++counter;
    return (static_cast<unsigned int>(std::rand()) << 8) ^ counter ^ 0x9E3779B9u;
}

std::string Fixed1(double v) {
    std::string s = std::to_string(v);
    const std::size_t dot = s.find('.');
    if (dot != std::string::npos && dot + 2 < s.size()) s.resize(dot + 2);
    return s;
}

} // namespace

void AiShapeTool::SetLabel(ai::ShapeLabel label) {
    if (label == m_label) return;
    m_label = label;
    // 라벨이 바뀌면 이전 도형의 미리보기는 의미가 없다. 선택 영역은 남겨두고 다시 뽑는다.
    if (m_state == State::Preview || m_state == State::Waiting) {
        if (m_cellW > 0 && m_cellH > 0) SendRequest(NextSeed());
        else                            Reset();
    }
}

void AiShapeTool::Reset() {
    m_state = State::Idle;
    m_cellW = m_cellH = 0;
    m_preview = ai::ShapeResult{};
    m_previewFhs.clear();
}

math::Rect AiShapeTool::SelectionWorldRect(const world::Map& map) const {
    const float ts = static_cast<float>(map.Tiles().TileSize());
    return {m_cellX * ts, m_cellY * ts, m_cellW * ts, m_cellH * ts};
}

void AiShapeTool::SendRequest(unsigned int seed) {
    if (m_generator == nullptr || !m_generator->Available()) {
        m_status = "AI 생성기가 연결되지 않았습니다";
        m_state  = State::Idle;
        return;
    }
    ai::ShapeRequest req;
    req.label = m_label;
    req.w     = m_cellW;
    req.h     = m_cellH;
    req.tile  = m_tile;
    req.seed  = seed;

    m_lastSeed = seed;
    m_generator->Request(req);
    m_state  = State::Waiting;
    m_status = std::string("생성 중… (") + ai::ShapeLabelName(m_label) + ")";
    m_previewFhs.clear();
}

void AiShapeTool::Accept(world::Map& map) {
    if (m_state != State::Preview || !m_preview.Valid()) return;

    const ai::ShapeApplyReport rep = ai::ApplyShape(map, m_preview, m_cellX, m_cellY);

    // 라벨을 맵에 남긴다 — 이 값이 학습 데이터의 정답이 된다(ADR-012).
    map.SetConcept(ai::ShapeLabelKey(m_label));
    // 출처도 남긴다 — 이 모델·seed면 같은 도형을 언제든 다시 뽑을 수 있다.
    map.SetAiGen(m_preview.model + " " + ai::ShapeLabelKey(m_label) + " " +
                 std::to_string(m_preview.seed) + " 1");

    m_status = std::string("확정: ") + ai::ShapeLabelName(m_label) +
               " — 타일 " + std::to_string(rep.tilesWritten) +
               ", 풋홀드 " + std::to_string(rep.footholdsAdded) +
               (rep.spawnMoved ? ", 스폰 이동" : "");
    Reset();
}

void AiShapeTool::Update(const platform::Input& in, const core::Camera& cam,
                         world::Map& map, bool allowMouse) {
    // --- 결과 수거 (워커 스레드가 넣어둔 것을 여기서 꺼낸다) ---
    if (m_generator != nullptr) {
        ai::ShapeResult got;
        while (m_generator->Poll(got)) {
            if (!got.ok) {
                m_status = got.error.empty() ? std::string("생성 실패") : got.error;
                m_state  = State::Idle;
                m_previewFhs.clear();
                continue;
            }
            m_preview         = std::move(got);
            m_lastSeed        = m_preview.seed;
            m_lastRoundTripMs = m_preview.roundTripMs;
            m_lastInferenceMs = m_preview.inferenceMs;
            m_state           = State::Preview;

            // 수락하면 생길 풋홀드를 지금 계산해 함께 보여준다 — "놀 수 있는가"가 한눈에 보인다.
            m_previewFhs = ai::ExtractFootholds(m_preview, m_cellX, m_cellY,
                                                map.Tiles().TileSize(), 1);
            m_status = std::string(ai::ShapeLabelName(m_label)) +
                       " — Enter 확정 / R 재생성 / Esc 취소   (seed " +
                       std::to_string(m_lastSeed) + ", " + Fixed1(m_lastRoundTripMs) + " ms)";
        }
    }

    // --- 키: 수락 / 재생성 / 취소 ---
    if (m_state == State::Preview) {
        if (in.WasPressed(platform::Key::Enter))  { Accept(map); return; }
        if (in.WasPressed(platform::Key::R))      { SendRequest(NextSeed()); return; }
        if (in.WasPressed(platform::Key::Escape)) { Reset(); m_status = "취소됨"; return; }
    }

    // --- 캔버스 드래그로 영역 선택 ---
    if (!allowMouse) {
        if (m_state == State::Selecting) m_state = State::Idle;
        return;
    }

    const world::TileMap& tiles = map.Tiles();
    const math::Vector2D w = cam.ScreenToWorld(in.MousePos());
    const int cx = std::clamp(tiles.CellX(w.x), 0, std::max(tiles.Width() - 1, 0));
    const int cy = std::clamp(tiles.CellY(w.y), 0, std::max(tiles.Height() - 1, 0));

    if (in.MousePressed(platform::MouseButton::Left)) {
        // 새 선택을 시작하면 이전 미리보기는 버린다.
        m_preview = ai::ShapeResult{};
        m_previewFhs.clear();
        m_dragAnchorX = cx;
        m_dragAnchorY = cy;
        m_state = State::Selecting;
    }

    if (m_state == State::Selecting) {
        m_cellX = std::min(m_dragAnchorX, cx);
        m_cellY = std::min(m_dragAnchorY, cy);
        m_cellW = std::abs(cx - m_dragAnchorX) + 1;
        m_cellH = std::abs(cy - m_dragAnchorY) + 1;

        if (in.MouseReleased(platform::MouseButton::Left)) {
            if (m_cellW < ai::kMinGridDim || m_cellH < ai::kMinGridDim) {
                m_status = "영역이 너무 작습니다 (최소 " + std::to_string(ai::kMinGridDim) +
                           "x" + std::to_string(ai::kMinGridDim) + "칸)";
                Reset();
            } else if (m_cellW > ai::kMaxGridDim || m_cellH > ai::kMaxGridDim) {
                m_status = "영역이 너무 큽니다 (최대 " + std::to_string(ai::kMaxGridDim) +
                           "x" + std::to_string(ai::kMaxGridDim) + "칸)";
                Reset();
            } else {
                SendRequest(NextSeed());
            }
        }
    }
}

void AiShapeTool::Render(platform::IRenderDevice& r, const core::Camera& cam,
                         const world::Map& map) const {
    const float ts = static_cast<float>(map.Tiles().TileSize());

    // 선택 사각형 — 드래그 중이거나 미리보기가 걸려 있을 때.
    if (m_cellW > 0 && m_cellH > 0 && m_state != State::Idle) {
        const math::Rect s = cam.WorldRectToScreen(SelectionWorldRect(map));
        r.DrawRect(s, kSelect);
        r.DrawRect({s.x - 1.0f, s.y - 1.0f, s.w + 2.0f, s.h + 2.0f}, kSelect);

        const std::string dim = std::to_string(m_cellW) + " x " + std::to_string(m_cellH);
        r.DrawText(dim, {s.x, s.y - 20.0f}, 16.0f, kSelect);
    }

    // 고스트 타일 — 반투명으로 덮어 "수락하면 이렇게 된다"를 보여준다. 아직 맵에는 없다.
    if (m_state == State::Preview && m_preview.Valid()) {
        for (int y = 0; y < m_preview.h; ++y) {
            for (int x = 0; x < m_preview.w; ++x) {
                const world::TileId id = m_preview.At(x, y);
                if (id == world::kEmptyTile) continue;
                const math::Rect cell{(m_cellX + x) * ts, (m_cellY + y) * ts, ts, ts};
                r.FillRect(cam.WorldRectToScreen(cell), GhostColor(id));
            }
        }

        // 수락 시 생길 풋홀드 — 도형이 "놀 수 있는가"를 확정 전에 보여준다.
        for (const world::Foothold& fh : m_previewFhs) {
            r.DrawLine(cam.WorldToScreen(fh.p1), cam.WorldToScreen(fh.p2), kFhGhost);
        }
    }
}

} // namespace gs::editor
