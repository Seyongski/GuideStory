#pragma once

#include "ai/AiTypes.h"
#include "ai/IShapeGenerator.h"
#include "core/Camera.h"
#include "math/Rect.h"
#include "platform/IRenderDevice.h"
#include "platform/Input.h"
#include "world/Foothold.h"
#include "world/Map.h"

#include <string>
#include <vector>

// AI 도형 생성 도구 — 에디터 캔버스에서 영역을 잡고, 생성 결과를 **고스트로 미리 보고**,
// 사람이 수락해야 맵에 들어간다.
//
// [ADR-010: AI는 제안하고 사람이 확정한다]
//   생성 결과를 맵에 직접 쓰지 않는 이유는 두 가지다. 검증되지 않은 모델이 자산을 직접
//   바꾸면 되돌리기가 어렵고, 사람이 고친 최종본이 다음 학습 데이터가 되어야 데이터
//   플라이휠이 성립한다.
//
// [상태 흐름]
//   Idle ──드래그──▶ Selecting ──놓음──▶ Waiting ──응답──▶ Preview
//                                            │                 │ Enter: 맵에 확정
//                                            │                 │ R    : 다른 seed로 재생성
//                                            └── 실패 ─────────┘ Esc  : 취소 → Idle
//
// 생성기는 인터페이스로만 받는다(소유하지 않는다). 스텁이든 실모델이든 libtorch든
// 이 도구는 구분하지 않는다 — 그게 G2에서 C++을 건드리지 않고 모델을 바꿀 수 있는 이유다.
namespace gs::editor {

class AiShapeTool {
public:
    enum class State { Idle, Selecting, Waiting, Preview };

    // 생성기를 붙인다(수명은 호출측이 관리). nullptr이면 도구가 비활성으로 동작한다.
    void Attach(ai::IShapeGenerator* generator) { m_generator = generator; }

    State          Phase() const { return m_state; }
    bool           HasPreview() const { return m_state == State::Preview; }
    ai::ShapeLabel Label() const { return m_label; }
    void           SetLabel(ai::ShapeLabel label);
    void           SetTile(world::TileId tile) { m_tile = tile; }
    world::TileId  Tile() const { return m_tile; }

    // 화면에 보여줄 한 줄 상태(생성기 이름/진행/실패 사유).
    const std::string& Status() const { return m_status; }
    // 마지막 성공 결과의 지연. 없으면 0.
    double LastRoundTripMs() const { return m_lastRoundTripMs; }
    double LastInferenceMs() const { return m_lastInferenceMs; }

    // 진행 중인 미리보기·선택을 버린다(모드를 벗어날 때 호출).
    void Reset();

    // 한 프레임. allowMouse=false면 캔버스 드래그를 무시한다(포인터가 UI 위).
    // 수락(Enter) 시에만 map을 바꾼다.
    void Update(const platform::Input& in, const core::Camera& cam,
                world::Map& map, bool allowMouse);

    // 선택 사각형 · 고스트 타일 · 예상 풋홀드 · 안내 문구.
    void Render(platform::IRenderDevice& r, const core::Camera& cam,
                const world::Map& map) const;

private:
    void SendRequest(unsigned int seed);
    void Accept(world::Map& map);
    math::Rect SelectionWorldRect(const world::Map& map) const;

    ai::IShapeGenerator* m_generator = nullptr;

    State          m_state = State::Idle;
    ai::ShapeLabel m_label = ai::ShapeLabel::Heart;
    world::TileId  m_tile  = 1;

    // 선택 영역(맵 셀 단위). x0,y0 = 좌상단, w,h = 칸 수.
    int m_cellX = 0, m_cellY = 0, m_cellW = 0, m_cellH = 0;
    int m_dragAnchorX = 0, m_dragAnchorY = 0;

    ai::ShapeResult              m_preview;      // Preview 상태에서만 유효
    std::vector<world::Foothold> m_previewFhs;   // 수락 시 생길 풋홀드(미리 보여준다)
    unsigned int                 m_lastSeed = 0;

    std::string m_status = "AI 생성기가 연결되지 않았습니다";
    double      m_lastRoundTripMs = 0.0;
    double      m_lastInferenceMs = 0.0;
};

} // namespace gs::editor
