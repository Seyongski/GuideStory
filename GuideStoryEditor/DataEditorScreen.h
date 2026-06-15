#pragma once

#include "EditorScreen.h"

#include "core/Ui.h"

#include <string>

namespace gs::app {

// 플레이어/스킬/몬스터/NPC 에디터가 공유하는 골격 화면(생성자에 표시 이름 주입).
// 선택 화면 → 빈 화면 → [새로 만들기]/[열기] → 편집(자리표시자) → [저장]의 흐름만 갖춘다.
// 데이터 모델·저장 포맷은 다음 단계(ADR-005 JSON + DataManager)에서 채운다.
class DataEditorScreen final : public EditorScreen {
public:
    explicit DataEditorScreen(std::string title);

    EditorScene Update(const platform::Input& in, float dt) override;
    void Render(platform::IRenderDevice& r) override;

private:
    std::string m_title;
    std::string m_status;
    bool        m_active = false; // 새로 만들기/열기 후 편집(자리표시자) 상태

    ui::Menu    m_emptyMenu;
    ui::Toolbar m_toolbar;
};

} // namespace gs::app
