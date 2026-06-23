#pragma once

#include "EditorScreen.h"

#include "core/HostLoop.h"
#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"

#include <memory>

namespace gs::app {

// 에디터 호스트 루프. 선택 화면(런처)에서 시작해 맵/플레이어/스킬/몬스터/NPC 에디터로 전환한다.
// 타이밍·폴·Present 골격은 core::HostLoop(Template Method)이 맡고, 이 클래스는
// 한 프레임의 의미(현재 화면 갱신·전환·렌더)만 Frame()으로 채운다.
// 항상 편집용(플레이 경로 없음) — 플레이는 GuideStoryGame.exe의 책임.
// 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다(ADR-006).
class EditorApp : public core::HostLoop {
public:
    EditorApp(platform::IWindow& window, platform::IRenderDevice& renderer);

protected:
    // 한 프레임: 현재 화면 갱신·전환·렌더. false면 앱 종료(장면이 Quit을 반환).
    bool Frame(const platform::Input& in, float dt) override;

private:
    static std::unique_ptr<EditorScreen> MakeScreen(EditorScene id);

    std::unique_ptr<EditorScreen> m_screen;
};

} // namespace gs::app
