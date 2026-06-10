# AGENTS.md — AI 에이전트 운영 헌장

> 이 파일은 GuideStory 프로젝트에서 작업하는 모든 AI 에이전트(코딩 에이전트 포함)의 **루트 진입 문서**입니다.
> 작업을 시작하기 전에 항상 이 문서를 먼저 읽고, 연결된 문서로 이동하세요.

## 0. 프로젝트 한 줄 요약

GuideStory — C++ / Visual Studio 기반 내러티브 게임. (상세: [PRODUCT.md](PRODUCT.md))

## 1. 문서 지도 (Harness Map)

| 문서 | 역할 | 언제 읽나 |
|------|------|-----------|
| [AGENTS.md](AGENTS.md) | 에이전트 운영 규칙(이 문서) | 항상 먼저 |
| [PRODUCT.md](PRODUCT.md) | 제품 비전·타깃·범위 | 무엇을 만드는지 알아야 할 때 |
| [ARCHITECTURE.md](ARCHITECTURE.md) | 시스템·코드 구조 | 코드를 바꾸기 전 |
| [PLANS.md](PLANS.md) | 로드맵·작업 백로그 | 다음 할 일을 정할 때 |
| [QUALITY_SCORE.md](QUALITY_SCORE.md) | 품질 기준·점수 루브릭 | 작업을 자가 평가할 때 |
| [RELIABILITY.md](RELIABILITY.md) | 안정성·오류 처리·테스트 정책 | 버그/크래시/회귀 다룰 때 |
| [tech-debt-tracker.md](tech-debt-tracker.md) | 기술 부채 목록 | 부채 발견/상환 시 |
| [docs/game-design.md](docs/game-design.md) | 게임 디자인 바이블 | 게임플레이/내러티브 작업 |
| [docs/mechanics-harness.md](docs/mechanics-harness.md) | 메카닉 구현 하네스 | 게임 메카닉 코딩 |
| [docs/product-specs.md](docs/product-specs.md) | 기능 단위 상세 스펙 | 특정 기능 구현 |

## 2. 작업 루프 (Operating Loop)

1. **이해** — 관련 문서를 읽고 작업의 의도를 파악한다.
2. **계획** — 변경 범위를 정의한다. 큰 작업은 [PLANS.md](PLANS.md)에 항목으로 남긴다.
3. **구현** — 주변 코드 스타일을 따른다. 작은 단위로 변경한다.
4. **검증** — 빌드/실행/테스트로 확인한다. ([RELIABILITY.md](RELIABILITY.md))
5. **자가 평가** — [QUALITY_SCORE.md](QUALITY_SCORE.md) 루브릭으로 점검한다.
6. **기록** — 부채는 [tech-debt-tracker.md](tech-debt-tracker.md)에, 결정은 해당 문서에 남긴다.

## 3. 기본 규칙 (Ground Rules)

- 되돌리기 어렵거나 외부로 나가는 작업은 먼저 확인한다.
- 문서와 코드가 충돌하면 **사실을 표면화**하고 추측으로 덮지 않는다.
- 테스트가 실패하면 실패 사실과 출력을 그대로 보고한다.
- 추측한 사실(파일/함수/플래그)은 사용 전에 실제 존재를 확인한다.
- 한 PR/커밋은 하나의 논리적 변경만 담는다.

## 4. 환경 메모

- 플랫폼: Windows / Visual Studio (`GuideStory.sln`, `GuideStory.vcxproj`)
- 언어: C++
- 빌드/실행 방법: (TODO — [ARCHITECTURE.md](ARCHITECTURE.md)에 채워넣기)
