# GuideStoryAI — 학습·추론 서브프로젝트

> 키워드 → 타일 도형 생성 모델의 **학습 코드와 추론 서버**. C++ 솔루션(`GuideStory.sln`) 밖의 독립 파이썬 프로젝트다.
> 설계 근거·결정은 [docs/ai-shape-synthesis.md](../docs/ai-shape-synthesis.md)에 있다. 이 문서는 **재현 절차**만 다룬다.
> 진행 순서와 조달 원칙(무엇을 직접 만들고 무엇을 가져다 쓰는가)은 [docs/ai-roadmap.md](../docs/ai-roadmap.md).
> 관련: [ARCHITECTURE.md](../ARCHITECTURE.md) ADR-007 · [PLANS.md](../PLANS.md) 2.5단계

## 0. 현재 상태

**G0(관통) 통과. G1(데이터) 진행 중** — 파서·합성 데이터·빌더 완료. 남은 것은 **손그림 수집**(사람이 그려야 한다).

착수 순서는 [docs/ai-roadmap.md](../docs/ai-roadmap.md) §3의 게이트를 따른다. **모델보다 에디터 관통(G0)이 먼저다.**

| 구성요소 | 파일 | 게이트 | 상태 |
|---|---|---|---|
| 추론 서버 (**스텁**) | `serve/ai_server.py` | G0 | ☑ 완료 (P-011) |
| 절차적 도형 래스터라이저 | `tools/synth_shapes.py` | G0 → G1 | ☑ 완료 (P-011) |
| `.gsmap` 파서 | `tools/gsmap.py` | G1 | ☑ 완료 (P-014) |
| 서버 프로브(디버깅 도구) | `serve/ai_client_probe.py` | G0 | ☑ 완료 (P-011) |
| 데이터셋 빌더·증강 | `tools/build_dataset.py` | G1 | ☑ 완료 (P-015) |
| CVAE 모델·학습 | `train/model.py`, `train/train.py` | G2 | ☐ 미착수 (P-016) |
| 채점용 분류기 | `train/classifier.py` | G2 | ☐ 미착수 (P-016) |
| 평가 리포트 | `train/eval.py` | G2 | ☐ 미착수 (P-016) |
| TorchScript export | `train/export.py` | G3 | ☐ 미착수 (P-017) |
| 풋홀드 추출 (**평가용 재구현**) | `tools/footholds.py` | G2 | ☐ 미착수 |
| 맵 → MapSpec 역산 | `tools/analyze.py` | G4 | ☐ 미착수 (P-021) |

상태 표기: ☐ 미착수 · ◐ 진행 중 · ☑ 완료(코드+리포트)

> **`serve/ai_server.py`가 가장 먼저다.** 첫 버전은 학습 모델 없이 `synth_shapes.py`의 래스터라이저로 격자를 만들어 돌려주는 스텁이고, 목적은 C++ 에디터와의 관통 검증이다. G2에서 이 파일의 생성 함수 하나만 실모델로 바꾼다 — **C++을 건드리게 되면 G0에서 계약을 잘못 그은 것이다.**
>
> **풋홀드 추출의 정본은 C++ 에디터에 있다.** 여기 `tools/footholds.py`는 플레이 가능성 지표 계산용 재구현이며, 같은 격자 입력에 대해 C++ 출력과 일치하는지 비교 테스트로 방어한다.

---

## 1. 왜 솔루션 밖에 두는가

C++ 프로젝트가 파이썬 환경에 의존하면 **엔진 빌드가 파이썬 설치를 요구**하게 된다. `GuideStoryServer`가 SDL/vcpkg에 의존하지 않게 분리한 것(ARCHITECTURE.md §3)과 같은 이유다.

두 세계의 계약은 딱 두 가지다.

1. **`.gsmap` 파일 포맷** — 파이썬이 읽고 쓰는 형식은 C++ `world::Map`이 읽고 쓰는 것과 동일해야 한다.
2. **추론 프로토콜** — `net/Framing`의 길이 프리픽스 + JSON 본문.

이 둘 외에 파이썬↔C++가 공유하는 것은 없다.

> **`tools/gsmap.py`는 C++ `Map.cpp`의 미러다.** 한쪽만 고치면 조용히 어긋난다 — `net/Protocol.h`를 서버·클라이언트가 함께 컴파일하는 것과 같은 위험이고, 여기선 언어가 달라 컴파일러가 잡아주지 못한다. **왕복 테스트(§5)가 유일한 방어선이다.**

---

## 2. 폴더 구조

```
GuideStoryAI/
├─ README.md                  # 이 문서
├─ requirements.txt
├─ tools/
│  ├─ gsmap.py                # .gsmap ↔ numpy 격자 (C++ Map.cpp의 미러)
│  ├─ synth_shapes.py         # 절차적 도형 래스터라이저 (합성 데이터 + 평가 베이스라인)
│  ├─ build_dataset.py        # raw/synth → 증강 → .npz 텐서
│  └─ footholds.py            # 생성 격자 → 풋홀드 선분 + 스폰 (결정론적 후처리)
├─ train/
│  ├─ model.py                # ShapeCVAE (v1) / ShapeTransformer (v2)
│  ├─ train.py                # 사전학습 → 파인튜닝
│  ├─ eval.py                 # 지표 측정 + 래스터라이저 대비 리포트
│  ├─ classifier.py           # 인식률 채점용 별도 분류기 (손그림으로만 학습)
│  └─ export.py               # → TorchScript (.pt)
├─ serve/
│  ├─ ai_server.py            # 추론 서버 (기본 127.0.0.1:7788)
│  └─ ai_client_probe.py      # 서버 프로브 — 규약 검증·디버깅 CLI
├─ data/                      # .gitignore
│  ├─ raw/<label>/*.gsmap     # 에디터로 직접 그린 것
│  ├─ synth/<label>/*.npy     # 절차 생성
│  ├─ feedback/*.gsmap        # AI 생성 후 사람이 수정한 최종본
│  └─ dataset/*.npz           # 학습용 텐서
├─ runs/                      # 체크포인트·로그·리포트 (.gitignore)
└─ models/
   └─ shape_cvae_v1.pt        # TorchScript — 커밋한다 (~1.2MB, libtorch가 로드)
```

**커밋 정책**: `data/`와 `runs/`는 제외, `models/*.pt`는 **포함**한다. 모델이 수 MB 수준이고, 이게 없으면 C++ 쪽 2단계 통합(libtorch)을 아무도 재현할 수 없다.

리포 루트 `.gitignore`에 추가할 항목:

```
GuideStoryAI/data/
GuideStoryAI/runs/
GuideStoryAI/.venv/
GuideStoryAI/**/__pycache__/
```

---

## 3. 환경 구성 ☑ 검증 완료 (2026-08-30)

CPU만으로 충분하다. v1 CVAE는 파라미터 30만 개 규모로, GPU 없이 10~20분이면 학습된다.

### 검증된 조합

| | 버전 | 비고 |
|---|---|---|
| Python | **3.12.10** | python.org 공식 빌드, 사용자 스코프 설치 |
| torch | **2.13.0+cpu** | CPU 휠. CUDA 불필요 |
| numpy / matplotlib / tqdm | 2.5.2 / 3.11.1 / 4.70.0 | `requirements.txt`에 고정 |

### 설치

```powershell
# Python 3.12 (사용자 스코프 — 관리자 권한 불필요)
winget install --id Python.Python.3.12 --scope user

py -3.12 -m venv GuideStoryAI\.venv
GuideStoryAI\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install --index-url https://download.pytorch.org/whl/cpu torch
python -m pip install -r GuideStoryAI\requirements.txt
```

### 배포판(Anaconda)이 아니라 순수 CPython을 쓰는 이유

콘다는 CUDA 툴킷 같은 **비파이썬 바이너리 의존**이 얽힐 때 값어치를 한다. 이 프로젝트가 쓰는 것은
PyTorch CPU 휠 + numpy + matplotlib뿐이고 전부 pip로 깔끔하게 설치된다 — 콘다의 이점이 없다.
반대로 G3에서 libtorch와 버전을 맞춰야 하는데, 환경이 단순할수록 그 짝을 추적하기 쉽다.

**Jupyter는 이 venv 안에 넣는다**(`pip install jupyterlab`). 다만 역할을 가른다:

| 용도 | 도구 |
|---|---|
| 파이프라인 (`gsmap.py`, `train.py`, `ai_server.py`) | **스크립트** |
| 탐색 — 증강 결과 확인, 학습 곡선, 라벨 임베딩 보간 그림 | **노트북** |

노트북을 파이프라인의 정본으로 만들지 않는다. 실행 순서가 숨겨져서 §7의 재현성 규칙과 정면으로
충돌하고, "어제는 됐는데 오늘은 안 되는" 학습 결과의 전형적 원인이 된다.

### ⚠️ libtorch 버전 고정

libtorch(C++ 배포용)는 **별개 다운로드**이며, 파이썬 `torch`와 버전이 맞아야 TorchScript 로드가
깨지지 않는다. G3(P-017) 착수 시 받을 것: **libtorch 2.13.0 CPU** (Debug/Release 각각 — MSVC는
런타임 라이브러리가 달라 섞이면 링크가 깨진다).

---

## 4. 파이프라인 실행 순서

### G0 — 관통 먼저 (학습 없음)

```powershell
# 스텁 서버: 절차적 래스터라이저로 격자를 만들어 소켓으로 반환한다.
# 이 시점에 학습 데이터도, 모델도, gsmap.py도 필요 없다.
python serve/ai_server.py --stub --port 7788
```

`GuideStoryEditor.exe`의 AI 패널에서 키워드를 넣어 고스트가 뜨고 Enter로 맵이 되면 G0 통과다. 그다음에야 아래로 넘어간다.

### G1~G3 — 데이터 · 학습 · 통합

```powershell
# ① 손그림 수집 — GuideStoryEditor.exe로 직접 그리고 라벨 폴더에 저장
#    맵 헤더에 CONCEPT <label> 을 넣는다. 라벨당 15장 이상.
#    하트는 합성 데이터를 쓰지 않으므로 여기서만 나온다 (설계 문서 §3.3).

# ② 합성 데이터 생성 — 라벨당 2,000장 (하트 제외) ☑
python tools\synth_shapes.py --out data\synth --per-label 2000

# ③ 데이터셋 빌드 (크롭 → 32x32 정규화 → 라벨별 증강) ☑
python tools\build_dataset.py --raw data\raw --synth data\synth --out data\dataset ^
       --montage data\dataset\preview.png

# ④ 학습 — 합성 사전학습 후 손그림 파인튜닝
python train/train.py --stage pretrain  --data data/dataset/synth.npz --out runs/pretrain
python train/train.py --stage finetune  --data data/dataset/raw.npz   --init runs/pretrain/best.ckpt --out runs/finetune

# ⑤ 평가 — 지표 + 래스터라이저 대비 비교표 + 라벨 보간 그림
python train/eval.py --ckpt runs/finetune/best.ckpt --out runs/finetune/report

# ⑥ TorchScript로 내보내기 (C++ libtorch가 로드)
python train/export.py --ckpt runs/finetune/best.ckpt --out models/shape_cvae_v1.pt

# ⑦ 스텁을 실모델로 교체 — 에디터 코드는 건드리지 않는다
python serve/ai_server.py --model models/shape_cvae_v1.pt --port 7788
```

**④의 두 단계 순서를 바꾸지 않는다.** 손그림 40장을 합성 2,000장과 섞어 한 번에 학습시키면, 내 그림체가 합성 데이터에 묻힌다.

---

## 5. `tools/gsmap.py` 계약 ☑ 완료 (P-014)

학습 파이프라인 전체가 이 파일 하나에 걸려 있다. 여기가 틀리면 나머지가 전부 조용히 틀린다.

```python
load(path)  -> Map      # 헤더 + tiles(np.uint8 [h][w]) + footholds + portals + concept
save(map, path) -> None # C++ world::Map이 읽을 수 있는 형식으로
crop_normalize(tiles, size=32) -> np.uint8[32][32]   # 타일 바운딩박스 → 최근접 리샘플
```

**완료 조건 — 무손실 왕복 ☑**

```powershell
python tools\gsmap.py --roundtrip ..\assets\maps\crystalgarden.gsmap ..\assets\maps\field01.gsmap
# [ OK ]  crystalgarden.gsmap  3705 bytes
# [ OK ]  field01.gsmap        2244 bytes
```

**핵심 제약 두 가지를 지켜서 바이트 동일이 나온다.**

| | 값 | 왜 |
|---|---|---|
| 줄바꿈 | **CRLF** | C++이 텍스트 모드 `ofstream`으로 쓴다. 파이썬은 `newline=""`로 열고 직접 넣는다 |
| float 서식 | `%g` (유효숫자 6) | C++ 기본 `ostream`과 같다. `1280.0` → `1280`, `255.764` → `255.764` |

그리고 **우리가 고치지 않은 블록은 원문 그대로 다시 내보낸다.** 값을 다시 계산해 쓰면 서식이 미묘하게 어긋나 왕복이 깨진다.

  · 소유(재생성): `TILES` `FOOTHOLDS` `SPAWN` `CONCEPT` `AIGEN` `SIZE` `TILESIZE` 버전
  · 원문 보존   : `PLAYERBOUNDS` `CAMERAVIEW` `BACKGROUND` `BGSIZE` `PORTALS` `OBJECTS` `MOBS`, **모르는 키 전부**

마지막 항목이 C++ 파서의 "모르는 태그는 건너뛴다"(P-010)와 짝이다. **양쪽 다 모르는 것을 잃지 않으므로**, 한쪽만 아는 필드가 생겨도 왕복에서 사라지지 않는다.

### 교차 언어 계약 검증 (양방향)

```
파이썬이 생성 → C++ Map::Load 가 읽음 → C++ 이 재저장 → 파이썬이 다시 읽음
```

CONCEPT·AIGEN·타일·풋홀드·스폰이 전부 유지되고, C++ 출력도 파이썬 왕복이 바이트 동일하다.
`AIGEN`은 이 검증 과정에서 **C++이 모르는 태그로 건너뛰어 재저장 시 사라지는 것을 발견**해
C++ 쪽에 보존 경로를 추가했다(`world::Map::AiGen`). 에디터로 열었다 저장했다고 "어떤
seed로 뽑았는지"가 사라지면 재현성 주장이 그 순간 깨진다.

### 파싱 대상 (`GSMAP 3` 기준)

| 키 | 파이썬이 쓰는가 | 비고 |
|---|---|---|
| `TILESIZE` / `SIZE` | ✅ | 격자 크기 |
| `TILES` | ✅ | 학습 입력 그 자체 |
| `FOOTHOLDS` | ✅ (쓰기만) | `tools/footholds.py`가 생성 |
| `SPAWN` | ✅ (쓰기만) | 후처리가 배치 |
| `CONCEPT` | ✅ | **라벨** — 학습 데이터의 정답 |
| `AIGEN` | ✅ (쓰기만) | 모델명/라벨/seed/리비전 — 재현성 |
| `BACKGROUND` / `BGSIZE` / `PLAYERBOUNDS` / `CAMERAVIEW` / `PORTALS` | ❌ | 원문 보존 후 그대로 출력 |

> `CONCEPT`와 "모르는 헤더 키 건너뛰기"는 **P-010에서 C++에 반영 완료**다(`GSMAP 3`). `AIGEN`은 아직 없지만, 건너뛰기 규칙 덕분에 버전을 올리지 않고 추가할 수 있다 (설계 문서 §2.3).

---

## 5.5. 데이터셋 빌드 ☑ (P-015)

출력은 `data/dataset/{synth,raw}.npz` + 같은 이름의 `.json`(재현 정보 — seed, 배수, 데이터셋 해시, git 커밋).

| | 장수 | 비고 |
|---|---|---|
| `synth.npz` | **10,000** (라벨당 2,000 × 5) | 사전학습용. **하트 0장** — 홀드아웃(§3.3) |
| `raw.npz` | 손그림 수 × `--raw-aug`(기본 40) | 파인튜닝용. 아직 비어 있다 |

**x는 0/1 이진 격자다 — 타일 번호는 버린다.** 모델이 배우는 것은 형태이지 색이 아니다.
같은 하트를 흙으로 그리든 물로 그리든 라벨은 하트인데, 타일 번호를 넣으면 둘을 다른
것으로 배운다.

### 증강이 두 곳에 있는 이유 (헷갈리기 쉬운 지점)

| 출처 | 증강 위치 | 기본 배수 |
|---|---|---|
| 합성 | **생성 시점** — `random_params(augment=True)`가 크기·비율·회전·두께·흔들림·드롭아웃을 랜덤화 | `--synth-aug 1` (더 늘리지 않는다) |
| 손그림 | **격자 수준** — `build_dataset.py`가 평행이동·반전·회전·스케일·두께·드롭아웃 적용 | `--raw-aug 40` (40장 → 1,600장) |

합성은 2,000장이 이미 서로 다르므로 또 증강하면 중복만 늘어난다.

### 라벨별 증강 정책이 실제로 강제된다

`AUG_POLICY`가 §3.2 표의 단일 출처이고, 정책에 묶인 변환만 `geometric()`으로 분리해
검증 가능하게 했다. **삼각형과 하트는 상하 반전이 0/300회**로 확인됐다 —
뒤집은 하트는 하트가 아니고, 증강을 라벨과 무관하게 적용하면 모델에게 거짓말을 가르친다.

### 눈으로 확인한다

`--montage preview.png`가 라벨별 표본을 한 장으로 뽑는다. 지표만 보면 "정책은 지켰는데
도형이 뭉개진" 데이터를 놓친다. 학습을 돌리기 전 한 번 보는 것이 가장 싼 검증이다.


## 6. 추론 프로토콜 ☑ 구현·검증 완료 (P-011)

### 와이어 포맷

전송 계층은 `GuideStory/src/net/Framing`과 **같은 규약**이다. 새로 설계하지 않은 이유는
TCP 경계 문제를 이미 한 번 풀어놨고, C++ 쪽(P-012)이 `TryExtractPacket`을 그대로 쓸 수
있어야 하기 때문이다.

```
[헤더 6바이트][바디 N바이트]
헤더: BodySize(uint32 LE) + Opcode(uint16 LE)   ← net::PacketHeader, #pragma pack(1)
바디: UTF-8 JSON
```

**두 가지는 의도적으로 다르다.**

| | 계정/채팅 채널 | AI 채널 | 이유 |
|---|---|---|---|
| 옵코드 공간 | `net::Opcode` | 독립 (1~4) | 다른 포트의 다른 엔드포인트다. 헤더 레이아웃만 같으면 프레이밍 코드는 공유된다 |
| 바디 상한 | 4 KiB | **1 MiB** | 4 KiB는 채팅 한 줄 기준이다. 32×32 격자 JSON이 그걸 넘는다. **상한을 검사한다는 원칙은 같고 값만 다르다** |

옵코드: `1` GenerateReq · `2` GenerateAck · `3` PingReq · `4` PingAck.
계정 서버와 같은 규칙 — **항상 끝에 추가한다.**

### 요청 / 응답

```json
{"v":1, "label":"heart", "w":32, "h":32, "tile":3, "seed":424242}
```

```json
{"v":1, "ok":true, "model":"stub-raster", "label":"heart", "seed":424242,
 "w":32, "h":32, "grid":[0,0,3,...], "inference_ms":0.16}
```

- **`grid`는 row-major로 평탄화한 `w*h` 배열**이다(중첩 배열이 아니다). C++ `ai::ShapeResult::grid`와 같은 배치라 그대로 `memcpy` 수준으로 옮겨진다.
- 실패는 예외가 아니라 응답이다: `{"ok":false, "error":"..."}`. **에디터는 AI가 실패해도 계속 동작해야 한다** — C++ 쪽은 `NullShapeGenerator`로 폴백한다.
- 미등록 키워드는 추측하지 않고 거절한다 (ADR-012).
- `w`/`h`는 4~128로 제한한다. **클라이언트가 보낸 값을 신뢰하지 않는다.**
- `seed`를 0으로 보내면 서버가 정하고 **실제로 쓴 값을 응답에 담는다** — 같은 도형을 다시 뽑을 수 있어야 하기 때문이다(맵 헤더 `AIGEN`에 기록).
- `inference_ms`는 서버 내부 시간. C++이 재는 왕복과의 차이가 **IPC 오버헤드**이고, 이게 ADR-007 증명 과제의 측정값이다. 현재 스텁 기준 왕복 평균 **0.43 ms**.

접속 주소는 C++ 쪽에서 `assets/config/ai.txt`로 읽는다 (없으면 기본 `127.0.0.1:7788`, 서버가 없어도 에디터는 정상 기동).

### 프로브로 확인하기

```powershell
python serve\ai_server.py --stub                    # 터미널 1
python serve\ai_client_probe.py --selftest          # 터미널 2 — 규약 17항목 검증
python serve\ai_client_probe.py --label heart       # 한 번 요청해서 ASCII로 보기
```

**P-012에서 C++ 클라이언트를 붙일 때 이 프로브가 판정 기준이다.** 프로브는 되는데
에디터가 안 되면 C++ 잘못이고, 프로브도 안 되면 서버 잘못이다.

---

## 7. 재현성 규칙

포트폴리오의 핵심은 "돌아간다"가 아니라 **"다시 돌려도 같은 결과가 나온다"** 이다.

- 모든 스크립트는 `--seed`를 받고, 기본값을 로그 첫 줄에 출력한다.
- `runs/<이름>/config.json`에 하이퍼파라미터·데이터셋 해시·git 커밋 해시를 남긴다.
- 평가 리포트는 **모델 체크포인트와 같은 폴더**에 둔다. 리포트와 모델이 흩어지면 어느 리포트가 어느 모델의 것인지 알 수 없게 된다.
- 생성된 맵은 헤더의 `AIGEN`에 모델명·seed가 박히므로, 그 두 값으로 언제든 같은 맵을 다시 뽑을 수 있어야 한다.
