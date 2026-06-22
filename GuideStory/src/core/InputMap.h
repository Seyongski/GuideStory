#pragma once

#include "platform/Input.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// 메이플식 키세팅의 데이터 모델: "행동(Action) → 물리 키" 바인딩.
// 게임 층은 platform::Key를 직접 읽지 않고 이 매핑을 통해 입력을 질의하므로,
// 사용자가 키를 재지정해도 게임 로직은 그대로다 (ADR-006: 입력 추상화의 연장).
// 방향키(좌·우 이동, ↑ 포탈, ↓ 드롭다운)는 고정이라 Action에 넣지 않는다.
namespace gs::core {

// 사용자가 키를 재지정할 수 있는 게임 행동.
// 현재 실제 동작하는 것은 Jump뿐이고 나머지는 향후 확장용 자리표시(빈 슬롯)다.
enum class Action {
    Jump,    // 점프 — 기본 Alt
    Attack,  // (자리표시 — 동작 미구현)
    PickUp,  // (자리표시)
    Skill1,  // (자리표시)
    Count
};

class InputMap {
public:
    InputMap() { ResetDefaults(); }

    // 기본 바인딩: 점프=Alt. 나머지 자리표시 행동은 미할당(빈 슬롯).
    void ResetDefaults();

    // 행동에 키를 지정한다. 재지정 불가한 키(IsAssignable=false)면 무시하고 false.
    // 같은 키를 이미 다른 행동이 쓰고 있으면 그 행동에서 해제한다(키 1개 = 행동 1개, 메이플식).
    bool Bind(Action a, platform::Key k);
    void Unbind(Action a) { m_bind[Idx(a)].reset(); }

    // 행동에 할당된 키(없으면 nullopt).
    std::optional<platform::Key> KeyFor(Action a) const { return m_bind[Idx(a)]; }

    // 키에 묶인 행동(역방향, UI 표시·해제용). 없으면 nullopt.
    std::optional<Action> ActionForKey(platform::Key k) const;

    // 바인딩을 텍스트 파일로 저장/로드. 성공 시 true.
    // 로드는 파일이 없거나 깨져도 던지지 않고 false만 반환한다(설정은 비필수 — 기본값 유지).
    bool Save(const std::string& path) const;
    bool Load(const std::string& path);

    // 행동 입력 질의 — 미할당 행동은 항상 false.
    bool IsDown(const platform::Input& in, Action a) const {
        const auto k = m_bind[Idx(a)];
        return k && in.IsDown(*k);
    }
    bool WasPressed(const platform::Input& in, Action a) const {
        const auto k = m_bind[Idx(a)];
        return k && in.WasPressed(*k);
    }

    // 재지정 가능한 키인지. 처음에는 QWER·Ctrl·Alt·Space만 허용한다.
    static bool IsAssignable(platform::Key k);

    // 재지정 가능한 키 목록(키세팅 UI의 키 칸 순서·단일 출처).
    static constexpr std::size_t kAssignableCount = 7;
    static const std::array<platform::Key, kAssignableCount>& AssignableKeys();

    // 행동 개수(UI 반복용).
    static constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

    // 향후 키세팅 UI/디버그용 라벨(화면 표시는 한글).
    static std::string_view Label(Action a);
    static std::string_view KeyLabel(platform::Key k);

private:
    static constexpr std::size_t Idx(Action a) { return static_cast<std::size_t>(a); }

    std::array<std::optional<platform::Key>, kActionCount> m_bind{};
};

} // namespace gs::core
