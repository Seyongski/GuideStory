#pragma once

#include "platform/IRenderDevice.h"
#include "platform/Input.h"

namespace gs::app {

// 캔버스(편집 공간) = 실제 게임 화면과 동일 크기. 그래야 에디터에서 보는 프레이밍이 게임과 1:1.
inline constexpr float kViewW = 1280.0f;
inline constexpr float kViewH = 720.0f;

// 상단 UI 스트립 높이(2행: 모드/파일 + 배경스케일/화면범위). UI는 캔버스를 덮지 않고 이 영역에만 둔다.
inline constexpr float kToolStripH = 80.0f;
// 에디터 창 크기 = 캔버스 + 상단 UI 스트립(캔버스를 침범하지 않도록 창을 키운다).
inline constexpr float kWinW = kViewW;
inline constexpr float kWinH = kToolStripH + kViewH;

// 에디터 장면(화면) 식별자. Update가 반환해 EditorApp 호스트 루프가 전환을 수행한다.
//  - Stay         : 현재 화면 유지
//  - Launcher     : 에디터 선택 화면(맵/플레이어/스킬/몬스터/NPC)
//  - MapEditor    : 맵 에디터(타일·풋홀드·스폰·포탈)
//  - PlayerEditor : 플레이어 에디터(골격)
//  - SkillEditor  : 스킬 에디터(골격)
//  - MonsterEditor: 몬스터 에디터(골격)
//  - NpcEditor    : NPC 에디터(골격)
//  - Quit         : 애플리케이션 종료
enum class EditorScene {
    Stay, Launcher, MapEditor, PlayerEditor, SkillEditor, MonsterEditor, NpcEditor, Quit
};

// 창 닫기(X) 요청에 대한 화면의 응답. Allow=종료 진행, Cancel=닫지 않고 계속.
enum class CloseDecision { Allow, Cancel };

// 한 에디터 화면의 공통 인터페이스(게임의 Screen과 동형). SDL을 모르며
// 인터페이스(IRenderDevice/Input)에만 의존한다(ADR-006).
class EditorScreen {
public:
    virtual ~EditorScreen() = default;

    // 이번 프레임 입력을 처리하고 전환 요청을 반환한다. EditorScene::Stay면 현재 화면 유지.
    virtual EditorScene Update(const platform::Input& in, float dt) = 0;

    // 백버퍼에 화면을 그린다(Clear 포함, Present는 EditorApp이 호출).
    virtual void Render(platform::IRenderDevice& r) = 0;

    // 창 닫기(X·강제 종료) 요청 시 호출. 저장 안 된 변경이 있으면 확인창을 띄워
    // CloseDecision::Cancel을 돌려 종료를 보류할 수 있다. 기본은 그대로 종료(Allow).
    virtual CloseDecision OnCloseRequest() { return CloseDecision::Allow; }
};

} // namespace gs::app
