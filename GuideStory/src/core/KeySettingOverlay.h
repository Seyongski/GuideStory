#pragma once

#include "core/InputMap.h"
#include "core/Ui.h"
#include "math/Rect.h"
#include "platform/IRenderDevice.h"
#include "platform/Input.h"

#include <array>
#include <optional>
#include <vector>

// 메이플식 키보드 설정 오버레이. 어떤 화면 위에도 떠서(App이 호스팅) 동작한다.
// 상호작용은 맵에디터의 오브젝트 배치처럼 "집어 들고(carry) → 클릭해 놓기":
//  - 트레이(미할당 행동)나 매핑된 키 칸을 좌클릭하면 그 행동이 마우스를 따라다닌다.
//  - 키 칸을 좌클릭하면 그 키에 놓는다. 그 키가 이미 점유돼 있으면 기존 행동이 마우스에 붙어 스왑된다.
//  - 키 칸 우클릭은 해제(행동이 트레이로 복귀), 들고 있을 때 우클릭은 취소.
// 편집은 참조로 받은 InputMap에 즉시 반영되고(바로 테스트 가능), 파일 저장은 App이 수행한다.
//
// 변동사항(dirty) 모델: "마지막 저장 상태(m_baseline)"와 현재 바인딩이 다르면 변동 상태.
//  - 변동 시 저장하기·원래대로 버튼이 활성화된다.
//  - 저장하기: 파일 저장 + baseline=현재 → 두 버튼 비활성.
//  - 원래대로: baseline(직전 저장본)으로 복귀 → 두 버튼 비활성.
//  - 닫기: 변동이 없으면 즉시 닫고, 있으면 "저장하시겠습니까?" 확인창(확인=저장 후 종료 / 취소=되돌리고 종료).
namespace gs::core {

class KeySettingOverlay {
public:
    explicit KeySettingOverlay(InputMap& bindings);

    bool Visible() const { return m_visible; }
    void Open(float screenW, float screenH); // 레이아웃 계산 후 표시
    void Close() { CancelCarry(); m_confirmClose = false; m_visible = false; }

    // 닫기 요청(\ 키·ESC). 변동이 있으면 확인창을 띄우고, 없으면 바로 닫는다.
    // 확인창이 떠 있을 때 다시 요청하면 취소(되돌리고 종료)로 처리한다.
    void RequestClose();

    // 한 프레임 입력 처리. 파일 저장이 필요하면 result.saveRequested=true(App이 기록).
    struct Result { bool saveRequested = false; };
    Result Update(const platform::Input& in);

    void Render(platform::IRenderDevice& r) const;

private:
    static constexpr std::size_t kKeyCount    = InputMap::kAssignableCount;
    static constexpr std::size_t kActionCount = InputMap::kActionCount;

    // 미할당이며 들고 있지도 않은 행동들 — 트레이에 왼쪽부터 채워 표시한다.
    std::vector<Action> TrayActions() const;

    // 이동 취소: 들고 있던 행동을 집어 든 원래 키로 되돌린다(없으면 트레이로).
    void CancelCarry();

    bool IsDirty() const;            // 현재 바인딩이 baseline과 다른가
    void CommitBaseline();           // baseline = 현재(저장 시점)
    void RevertToBaseline();         // 현재 = baseline(되돌리기)

    InputMap& m_bindings;
    InputMap  m_baseline;            // 마지막 저장 상태 스냅샷(dirty 비교·되돌리기 기준)
    bool      m_visible = false;
    bool      m_confirmClose = false; // 닫기 확인창 표시 중
    float     m_screenW = 0.0f;
    float     m_screenH = 0.0f;

    math::Rect                            m_panel{};
    std::array<math::Rect, kKeyCount>     m_keyCell{};   // 키 칸(AssignableKeys 순서)
    std::array<math::Rect, kActionCount>  m_chipSlot{};  // 트레이 슬롯 위치(왼쪽부터 채움)
    app::ui::Toolbar                      m_buttons;     // 저장하기/원래대로/초기화/닫기
    math::Rect                            m_confirmBox{}; // 닫기 확인창 영역
    app::ui::Toolbar                      m_confirmButtons; // 확인/취소

    std::optional<Action>        m_carried;              // 마우스를 따라다니는(집어 든) 행동
    std::optional<platform::Key> m_carriedFrom;          // 집어 든 원래 키(취소 시 복귀 대상; 트레이/스왑은 nullopt)
    math::Vector2D               m_cursorPos{};           // 마지막 마우스 위치(고스트 렌더용)
};

} // namespace gs::core
