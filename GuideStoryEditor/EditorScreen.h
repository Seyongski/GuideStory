#pragma once

#include "platform/IRenderDevice.h"
#include "platform/Input.h"

namespace gs::app {

// 1차: 윈도우 크기 고정(IWindow에 크기 질의 추가 전까지 상수). 화면 레이아웃의 기준.
inline constexpr float kViewW = 1280.0f;
inline constexpr float kViewH = 720.0f;

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

// 한 에디터 화면의 공통 인터페이스(게임의 Screen과 동형). SDL을 모르며
// 인터페이스(IRenderDevice/Input)에만 의존한다(ADR-006).
class EditorScreen {
public:
    virtual ~EditorScreen() = default;

    // 이번 프레임 입력을 처리하고 전환 요청을 반환한다. EditorScene::Stay면 현재 화면 유지.
    virtual EditorScene Update(const platform::Input& in, float dt) = 0;

    // 백버퍼에 화면을 그린다(Clear 포함, Present는 EditorApp이 호출).
    virtual void Render(platform::IRenderDevice& r) = 0;
};

} // namespace gs::app
