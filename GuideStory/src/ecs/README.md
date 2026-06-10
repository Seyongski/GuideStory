# ecs/ — 컴포넌트 기반 게임 오브젝트 (ADR-003)

플레이어·몬스터·아이템 등 게임 오브젝트의 구조.

- 초기에는 가벼운 상속으로 시작하되, 스킬·몬스터 시스템 확장 시점에 **컴포넌트 패턴** 도입.
- 예: Transform, Sprite, Collider, Velocity, SkillState 등 컴포넌트 조합.
- 증명 과제: 상속→컴포넌트 전환 동기와 장단점 비교 기록. (→ [../../../ARCHITECTURE.md](../../../ARCHITECTURE.md) ADR-003)

> SDL 비의존 (불변 규칙). 좌표/영역은 `math/Vector2D`, `math/Rect` 사용.
