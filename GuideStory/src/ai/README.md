# ai/ — 몬스터 FSM & AI 추론 (ADR-007)

- **FSM 기반 몬스터** 행동: Idle / Patrol / Chase / Attack / Hit / Dead 등. 파라미터는 `data/`에서 주입.
- 2.5단계: Python(학습) ↔ C++ 엔진 통신(ZMQ/Socket), 학습된 모델을 **libtorch**로 C++에서 직접 로드·추론.
- 증명 과제: 통신 레이어 지연 측정, 오프라인 학습→온라인 추론 전환 기록. (→ ADR-007)

> SDL 비의존. 게임 상태만 읽고 행동 결정을 출력.
