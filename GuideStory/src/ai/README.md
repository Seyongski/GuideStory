# ai/ — AI 도형 생성 & 몬스터 FSM (ADR-007, ADR-010~015)

> SDL 비의존. 게임/편집 상태만 읽고 결정을 출력한다.
> 설계: [docs/ai-roadmap.md](../../../docs/ai-roadmap.md)(조달 원칙·게이트) ·
> [docs/ai-shape-synthesis.md](../../../docs/ai-shape-synthesis.md)(키워드 → 타일 도형)
> 짝이 되는 파이썬 쪽: [GuideStoryAI/](../../../GuideStoryAI/README.md)

## 현재 있는 것 (P-010 · P-012 · P-013)

| 파일 | 역할 |
|---|---|
| `AiTypes.h` | `ShapeLabel`(닫힌 집합 + 동의어 파싱), `ShapeRequest`, `ShapeResult`. 순수 데이터 |
| `AiProtocol.h` | 와이어 규약 — 옵코드, 바디 상한, 기본 포트. 파이썬 `ai_server.py`와 짝 |
| `AiConfig.h` | `assets/config/ai.txt` 로드 (`net::ServerConfig`와 같은 포맷) |
| `Json.h` | 최소 JSON 리더/라이터 (외부 의존 없음) |
| `IShapeGenerator.h` | 생성기 인터페이스 — **비동기**(`Request` → `Poll`) |
| `NullShapeGenerator.h` | AI 미연결 폴백. 에디터가 AI 없이 완결됨을 코드로 보증 |
| `RemoteShapeGenerator.*` | 소켓 + JSON으로 파이썬 추론 서버 질의. 워커 스레드 + 결과 큐 |
| `ShapeToMap.*` | 격자 → 타일 + **풋홀드** + 스폰. 결정론적 후처리(ADR-011) |

## 불변 규칙

1. **라벨 열거자는 뒤에만 추가한다.** 순서가 학습 모델의 라벨 인덱스다 — 중간에 끼우면
   학습된 체크포인트의 임베딩이 통째로 어긋나고, 그 사고는 런타임까지 조용히 간다
   (`net/Protocol.h` 옵코드와 같은 규칙). 옵코드도 마찬가지다.
2. **모르는 키워드는 추측하지 않는다.** `ParseShapeLabel`은 false를 돌려준다 (ADR-012).
3. **실패는 예외가 아니라 결과다.** `ShapeResult{ok=false, error}`. AI는 부가 기능이므로
   실패가 편집 흐름을 끊으면 안 된다.
4. **`net/Socket.h`를 이 폴더의 헤더에 포함하지 않는다.** winsock2.h가 SDL을 쓰는 TU에
   섞이면 재정의 오류가 난다. 소켓은 `RemoteShapeGenerator.cpp` 안에만 존재한다
   (`net/NetClient.h`와 같은 이유).
5. **생성한 맵에는 출처(`AIGEN`)를 남기고, 로드 시 보존한다.** 모델명·seed가 있어야 같은
   도형을 다시 뽑을 수 있다. 에디터로 열었다 저장했다고 사라지면 재현성 주장이 깨진다.
6. **응답은 프로세스 밖에서 온다.** 격자 크기·길이·타일 번호 범위를 쓰기 전에 전부 검사한다.
   `net/Framing`이 선언된 길이를 신뢰하기 전에 상한을 검사하는 것과 같은 이유다.

## 프레이밍 재사용

`net/Framing`을 그대로 쓴다. TCP 경계 문제를 두 번 푸는 것은 두 번 틀릴 기회다.
다만 채널마다 바디 상한이 달라야 해서 `TryExtractPacket`/`BuildPacket`에
`maxBodySize` 매개변수를 더했다(기본값은 기존 `kMaxBodySize`라 계정/채팅 호출부는 그대로).

| | 계정/채팅 | AI |
|---|---|---|
| 옵코드 공간 | `net::Opcode` | `ai::AiOpcode` (독립) |
| 바디 상한 | 4 KiB (채팅 한 줄) | 1 MiB (32×32 격자 JSON) |

**상한을 검사한다는 원칙은 같고 값만 다르다.** 값을 늘리려고 검사를 빼지 않는다.

## 측정된 지연 (스텁 서버 기준, 루프백)

| 빌드 | 왕복 평균 | 서버 내부 | IPC 오버헤드 |
|---|---|---|---|
| Debug | 3.22 ms | 0.18 ms | 3.05 ms |
| Release | **0.91 ms** | 0.19 ms | **0.71 ms** |

Debug가 3배 이상 느린 것은 대부분 `Json.h`의 1024원소 배열 파싱 비용이다(`Value`가
문자열+벡터를 품는 무거운 타입이라 최적화 없이는 할당이 그대로 드러난다). **ADR-007
비교표에는 Release 수치를 쓴다.** 첫 요청만 TCP 접속 때문에 200ms 안팎이고, 연결을
유지하므로 이후는 위 값이다.

## 예정

| 파일 | 게이트 | 역할 |
|---|---|---|
| `TorchShapeGenerator.*` | G3 (P-017) | libtorch로 TorchScript 인프로세스 추론 (`#ifdef GUIDESTORY_WITH_TORCH`) |
| 몬스터 FSM | 2단계 (P-005) | Idle / Patrol / Chase / Attack / Hit / Dead. 파라미터는 `data/`에서 주입 |

`RemoteShapeGenerator`와 `TorchShapeGenerator`를 **같은 인터페이스 뒤에 두는 것**이
ADR-007 증명 과제(IPC vs 인프로세스 지연 비교)의 전제다. 하나만 만들면 비교할 대상이 없다.
`ShapeResult`의 `roundTripMs`/`inferenceMs` 두 필드가 그 측정값이고, 차이가 곧 IPC 오버헤드다.
