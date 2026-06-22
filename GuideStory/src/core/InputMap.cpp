#include "core/InputMap.h"

#include <fstream>
#include <string>

namespace gs::core {

using platform::Key;

namespace {
// 파일 저장용 안정 토큰(화면 라벨 Label()과 달리 ASCII·불변).
std::string_view ActionToken(Action a) {
    switch (a) {
        case Action::Jump:   return "Jump";
        case Action::Attack: return "Attack";
        case Action::PickUp: return "PickUp";
        case Action::Skill1: return "Skill1";
        default:             return "?";
    }
}
std::optional<Action> ParseAction(std::string_view s) {
    for (std::size_t i = 0; i < static_cast<std::size_t>(Action::Count); ++i) {
        const auto a = static_cast<Action>(i);
        if (ActionToken(a) == s) return a;
    }
    return std::nullopt;
}
std::optional<Key> ParseKey(std::string_view s) {
    for (Key k : InputMap::AssignableKeys())
        if (InputMap::KeyLabel(k) == s) return k;
    return std::nullopt; // "-" 등 미할당/미지원 토큰
}
} // namespace

void InputMap::ResetDefaults() {
    m_bind = {}; // 전부 미할당으로 초기화
    m_bind[Idx(Action::Jump)] = Key::LAlt; // 점프 기본값: Alt (이전 Space에서 변경)
    // Attack/PickUp/Skill1은 빈 슬롯으로 둔다 — 키세팅에서 사용자가 지정.
}

const std::array<Key, InputMap::kAssignableCount>& InputMap::AssignableKeys() {
    // 키세팅 화면의 키 칸 순서이자 IsAssignable의 단일 출처.
    static const std::array<Key, kAssignableCount> kKeys = {
        Key::Q, Key::W, Key::E, Key::R, Key::LCtrl, Key::LAlt, Key::Space};
    return kKeys;
}

bool InputMap::IsAssignable(Key k) {
    // 처음에는 QWER·Ctrl·Alt·Space만 행동에 지정할 수 있다(방향키 등은 고정).
    for (Key a : AssignableKeys())
        if (a == k) return true;
    return false;
}

std::optional<Action> InputMap::ActionForKey(Key k) const {
    for (std::size_t i = 0; i < kActionCount; ++i)
        if (m_bind[i] == k) return static_cast<Action>(i);
    return std::nullopt;
}

bool InputMap::Save(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << "GSKEYS 1\n";
    for (std::size_t i = 0; i < kActionCount; ++i) {
        const auto a = static_cast<Action>(i);
        const auto k = m_bind[i];
        out << ActionToken(a) << " " << (k ? KeyLabel(*k) : "-") << "\n";
    }
    out << "END\n";
    return static_cast<bool>(out);
}

bool InputMap::Load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;

    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "GSKEYS") return false;

    // 로컬에 먼저 채우고 성공 시에만 교체(부분 손상에도 기존 바인딩 보존).
    std::array<std::optional<Key>, kActionCount> loaded{};
    while (in >> tag) {
        if (tag == "END") break;
        std::string keyTok;
        if (!(in >> keyTok)) return false;
        const auto a = ParseAction(tag);
        if (!a) continue;                 // 미래/미지원 행동은 건너뜀(상위 호환)
        loaded[Idx(*a)] = ParseKey(keyTok); // "-"·미지원 → nullopt(미할당)
    }
    m_bind = loaded;
    return true;
}

bool InputMap::Bind(Action a, Key k) {
    if (!IsAssignable(k)) return false;
    // 같은 키를 쓰던 다른 행동을 해제 (키 1개 = 행동 1개).
    for (auto& slot : m_bind)
        if (slot == k) slot.reset();
    m_bind[Idx(a)] = k;
    return true;
}

std::string_view InputMap::Label(Action a) {
    switch (a) {
        case Action::Jump:   return "점프";
        case Action::Attack: return "공격";
        case Action::PickUp: return "줍기";
        case Action::Skill1: return "스킬1";
        default:             return "?";
    }
}

std::string_view InputMap::KeyLabel(Key k) {
    switch (k) {
        case Key::Q:      return "Q";
        case Key::W:      return "W";
        case Key::E:      return "E";
        case Key::R:      return "R";
        case Key::LCtrl:  return "Ctrl";
        case Key::LAlt:   return "Alt";
        case Key::Space:  return "Space";
        default:          return "";
    }
}

} // namespace gs::core
