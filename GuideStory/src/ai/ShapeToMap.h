#pragma once

#include "ai/AiTypes.h"
#include "world/Foothold.h"
#include "world/Map.h"

#include <vector>

// 생성된 타일 격자를 **플레이 가능한 맵 조각**으로 바꾸는 결정론적 후처리.
//
// [왜 AI가 아니라 규칙인가 — ADR-011]
//   ADR-008에 따라 타일은 코스메틱이고 충돌은 풋홀드다. 그러니 격자만 새기면 맵이 아니라
//   그림이다. 풋홀드까지 학습시키려면 데이터가 수십 배 필요한데, 이건 규칙으로 정확히
//   풀린다. 실패 지점도 갈린다 — 도형이 이상하면 모델 탓, 못 밟으면 이 파일 탓이다.
//
// [정본은 여기다]
//   파이썬 쪽 tools/footholds.py 는 평가 지표(플레이 가능성) 계산용 재구현이며,
//   같은 격자 입력에 대한 출력 일치를 테스트로 방어한다.
namespace gs::ai {

struct ShapeApplyOptions {
    bool addFootholds = true;  // 표면에서 풋홀드를 뽑아 추가한다
    bool moveSpawn    = true;  // 가장 긴 발판 위로 스폰을 옮긴다
    int  minRunCells  = 2;     // 이보다 짧은 파편 선분은 버린다(걸리적거리는 턱이 된다)
};

struct ShapeApplyReport {
    int  tilesWritten   = 0;
    int  footholdsAdded = 0;
    bool spawnMoved     = false;
};

// 격자에서 표면 풋홀드를 계산한다. **맵을 바꾸지 않는다** — 미리보기와 테스트가 같은 함수를 쓴다.
//
// 알고리즘:
//   1. 열마다 표면 높이 = 위 칸이 빈 타일 셀 중 가장 위
//   2. 열을 좌→우로 훑어 폴리라인을 만든다. 열이 비었거나 높이차가 2칸 이상이면(절벽) 끊는다
//   3. 일직선 위의 중간 점을 제거한다 → 평지는 선분 하나, 경사는 선분 하나, 곡면은 여러 개
//   4. minRunCells 보다 짧은 조각은 버린다
//
// 3번이 "높이차 1칸이면 경사로 잇는다"는 규칙의 실체다. 원·하트의 곡면이 계단이 아니라
// 경사가 되고, 메이플식 경사 보행이 자연스럽게 나온다(ADR-008).
std::vector<world::Foothold> ExtractFootholds(const ShapeResult& shape,
                                              int cellX, int cellY, int tileSize,
                                              int nextId, int minRunCells = 2);

// 격자를 맵의 (cellX, cellY) 칸 위치에 새기고, 옵션에 따라 풋홀드·스폰을 채운다.
// 맵 밖으로 나가는 칸은 조용히 버린다(에디터가 경계를 넘겨 붙여도 안전해야 한다).
ShapeApplyReport ApplyShape(world::Map& map, const ShapeResult& shape,
                            int cellX, int cellY, const ShapeApplyOptions& opt = {});

} // namespace gs::ai
