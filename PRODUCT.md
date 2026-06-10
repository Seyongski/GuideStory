# PRODUCT.md — 제품 정의

## 1. 비전 (Vision)

> **GuideStory** 는 메이플스토리류 2D 사이드스크롤링 플랫포머 RPG를 **클라이언트/서버가 분리된 데디케이트 서버 구조**로 구현하는 자작 게임 엔진이다.
> 단순 게임 완성이 아니라, **구조적 설계와 컴퓨터 과학 이론의 실제 적용 및 그 한계를 데이터로 증명**하는 것을 목적으로 한다.

이 프로젝트는 **포트폴리오**다. 모든 핵심 기술 선택은 "구현했다"로 끝나지 않고,
**왜 선택했는지·트레이드오프·한계·벤치마크**를 문서로 남긴다. (→ [ARCHITECTURE.md](ARCHITECTURE.md)의 ADR/증명 과제)

## 2. 타깃 (Target)

- **1차 목표**: 메이플스토리 **콘텐츠 프로그래밍 직무** 지원용 역량 증명 포트폴리오.
- **증명하려는 역량**:
  - 네트워크 프로그래밍 (WinSock, IOCP/Select)
  - 논리적 사고 및 알고리즘 (QuadTree, FSM, 충돌 처리)
  - 멀티스레드 프로그래밍
  - 게임 클라이언트/서버 구조 (서버 권위 검증)

## 3. 핵심 컨셉 (Core Concept)

- 2D 사이드스크롤링 플랫포머를 기본 베이스로 한다.
- **반드시 클라이언트와 서버가 분리된 구조**로 설계한다. (클라이언트는 입력 전송·렌더만, 판정은 서버)
- 클라이언트/서버 경계 없는 **풀스택** 개발 + 2D 플랫포머 장르 **신기술 리서치/PoC**.

## 4. 기술 스택 (요약 — 상세는 ARCHITECTURE.md)

| 영역 | 선택 |
|------|------|
| IDE / 표준 | Visual Studio 2022, **C++20** |
| 클라이언트 그래픽 | **SDL2** (경량 2D 라이브러리) |
| 네트워크 | C++ **WinSock** (IOCP 또는 Select 모델) |
| AI 학습 | Python (ML 에코시스템) → **libtorch** 로 C++ 추론 |
| C++ ↔ Python 통신 | ZMQ 또는 일반 Socket |
| 데이터 | 외부 **JSON / CSV** 파일 (데이터 주도) |

## 5. 범위 (Scope)

### In Scope
- [ ] 로컬 2D 플랫포머 (타일맵, 이동, 점프 물리)
- [ ] FSM 기반 몬스터, AABB 충돌, QuadTree 최적화
- [ ] JSON/CSV 데이터 주도 스킬·아이템·몬스터 시스템
- [ ] C++ ↔ Python AI 서브시스템 통합 (libtorch 추론)
- [ ] WinSock 멀티플레이어 데디케이트 서버 (동기화 검증)

### Out of Scope (현 버전)
- [ ] 상용 수준 콘텐츠 분량 / 라이브 운영 인프라
- [ ] 결제·계정 등 백오피스
- [ ] 모바일/콘솔 포팅

## 6. 성공 지표 (Success Metrics)

이 프로젝트의 "성공"은 게임 완성도가 아니라 **증명의 질**로 측정한다.

- **정량**: 브루트포스 vs QuadTree 프레임타임(ms)·CPU 점유율 비교 데이터, 멀티플레이어 N명 동기화 검증.
- **정성**: 각 ADR의 트레이드오프/한계 분석 문서, SDL2→SDL3 이식성(결합도) 평가 보고.
- 모든 "지침 지표"(증명 과제)가 문서로 채워졌는가. (→ [ARCHITECTURE.md](ARCHITECTURE.md) §증명 과제 대시보드)

## 7. 관련 문서

- 아키텍처·ADR·증명 과제: [ARCHITECTURE.md](ARCHITECTURE.md)
- 개발 단계 로드맵: [PLANS.md](PLANS.md)
- 게임 디자인: [docs/game-design.md](docs/game-design.md)
- 메카닉 구현 하네스: [docs/mechanics-harness.md](docs/mechanics-harness.md)
- 기능 스펙: [docs/product-specs.md](docs/product-specs.md)
