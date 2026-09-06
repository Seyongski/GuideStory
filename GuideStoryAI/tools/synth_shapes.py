"""절차적 도형 래스터라이저 — 이 파일은 세 가지 역할을 겸한다.

  1. **G0 스텁**: 학습 모델이 없는 동안 serve/ai_server.py 가 이걸로 격자를 만들어 돌려준다.
  2. **평가 베이스라인**: 학습 모델과 나란히 채점한다 (docs/ai-shape-synthesis.md §5.2).
  3. **합성 데이터 생성기**: 사전학습용 2,000장/라벨을 여기서 뽑는다 (§3.1).

셋을 한 파일로 두는 이유는 셋이 같은 것이어야 하기 때문이다. 스텁과 베이스라인이 다른
코드면 "모델이 베이스라인을 이겼다"는 비교가 성립하지 않는다.

주의: 하트는 **합성 데이터를 만들지 않는다** (§3.3의 홀드아웃 증명 설계). 여기 구현이
있는 것은 스텁·베이스라인 용도이며, generate_dataset() 은 하트를 제외한다.
"""
from __future__ import annotations

import argparse
import math
import os
from dataclasses import dataclass

import numpy as np

# 순서 = ai/AiTypes.h 의 ShapeLabel 열거자와 **정확히 같아야 한다**.
# 이 순서가 학습 모델의 라벨 인덱스이므로 중간에 끼우지 않고 뒤에만 추가한다.
LABELS = ["square", "circle", "triangle", "cross", "star", "heart"]
LABEL_INDEX = {name: i for i, name in enumerate(LABELS)}

# 합성 데이터에서 제외하는 라벨 — 손그림만으로 학습시켜 일반화를 증명한다.
HOLDOUT_LABELS = {"heart"}


@dataclass
class ShapeParams:
    """도형 하나의 생김새. 랜덤화하면 그대로 데이터 증강이 된다."""
    scale: float = 0.88      # 격자 대비 크기 (1.0 = 꽉 참)
    aspect: float = 1.0      # 가로/세로 비
    cx: float = 0.0          # 중심 오프셋 (-1..1 정규 좌표)
    cy: float = 0.0
    rotation: float = 0.0    # 라디안
    thickness: int = 0       # 0 = 채움, >0 = 윤곽선 두께(칸)
    jitter: float = 0.0      # 경계 흔들림 강도
    dropout: float = 0.0     # 셀 드롭아웃 비율


# --------------------------------------------------------------------------- #
# 내부 헬퍼
# --------------------------------------------------------------------------- #

def shift(mask: np.ndarray, dy: int, dx: int) -> np.ndarray:
    """격자 밖은 False로 채우며 이동. 침식/팽창의 기본 연산이다.

    build_dataset.py 의 격자 증강(두께 ±1, 평행이동)도 이 함수를 쓴다 —
    같은 연산이 두 곳에 각자 구현돼 있으면 조용히 달라진다.
    """
    out = np.zeros_like(mask)
    h, w = mask.shape
    ys, yd = (slice(0, h - dy), slice(dy, h)) if dy >= 0 else (slice(-dy, h), slice(0, h + dy))
    xs, xd = (slice(0, w - dx), slice(dx, w)) if dx >= 0 else (slice(-dx, w), slice(0, w + dx))
    out[yd, xd] = mask[ys, xs]
    return out


def erode(mask: np.ndarray, r: int) -> np.ndarray:
    """정사각 구조요소로 침식. scipy 없이 시프트 AND 만으로 한다(의존성 최소)."""
    out = mask.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dy or dx:
                out &= shift(mask, dy, dx)
    return out


def dilate(mask: np.ndarray, r: int) -> np.ndarray:
    """정사각 구조요소로 팽창(침식의 쌍대). 윤곽 두께 증강에 쓴다."""
    out = mask.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dy or dx:
                out |= shift(mask, dy, dx)
    return out


def _polygon_mask(u: np.ndarray, v: np.ndarray, pts) -> np.ndarray:
    """짝수-홀수 광선 교차 판정으로 다각형 내부를 채운다."""
    inside = np.zeros(u.shape, dtype=bool)
    n = len(pts)
    for i in range(n):
        x1, y1 = pts[i]
        x2, y2 = pts[(i + 1) % n]
        if y1 == y2:                       # 수평 변은 제외 (0으로 나누기 방지)
            continue
        straddles = (y1 > v) != (y2 > v)   # 이 변이 점의 y를 걸치는가
        xint = (x2 - x1) * (v - y1) / (y2 - y1) + x1
        inside ^= straddles & (u < xint)
    return inside


def _star_points(n: int = 5, inner: float = 0.382):
    """꼭짓점 n개 별. 위쪽(-y)이 뾰족하게 시작한다.

    5각 별은 위 꼭짓점(y=-1)과 아래 두 꼭짓점(y=+0.81)이 비대칭이라, 그대로 두면
    도형이 상자 위쪽으로 치우친다. 비율은 유지한 채(등방 스케일) 중심만 맞춘다 —
    축별로 늘리면 별이 찌그러진다.
    """
    pts = []
    for i in range(n * 2):
        r = 1.0 if i % 2 == 0 else inner
        a = -math.pi / 2.0 + i * math.pi / n
        pts.append((r * math.cos(a), r * math.sin(a)))

    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    cx, cy = (max(xs) + min(xs)) / 2.0, (max(ys) + min(ys)) / 2.0
    scale = max((max(xs) - min(xs)) / 2.0, (max(ys) - min(ys)) / 2.0)
    return [((x - cx) / scale, (y - cy) / scale) for x, y in pts]


def _filled_mask(label: str, u: np.ndarray, v: np.ndarray) -> np.ndarray:
    """정규 좌표(u,v)에서 도형 내부 판정. 도형은 대략 [-1,1]^2 을 채운다."""
    if label == "square":
        return (np.abs(u) <= 1.0) & (np.abs(v) <= 1.0)

    if label == "circle":
        return (u * u + v * v) <= 1.0

    if label == "triangle":
        return _polygon_mask(u, v, [(0.0, -1.0), (-1.0, 1.0), (1.0, 1.0)])

    if label == "cross":
        arm = 1.0 / 3.0
        return (((np.abs(u) <= arm) & (np.abs(v) <= 1.0)) |
                ((np.abs(u) <= 1.0) & (np.abs(v) <= arm)))

    if label == "star":
        return _polygon_mask(u, v, _star_points())

    if label == "heart":
        # 고전적 음함수 (x^2+y^2-1)^3 - x^2*y^3 <= 0.
        # 이 곡선의 실제 범위는 x in [-1.13, 1.13], y in [-1.26, +1.22] 로 y가 비대칭이다
        # (아래 꼭지가 위 봉우리보다 조금 더 멀다). 화면 y는 아래로 증가하므로 뒤집으면서
        # 그 비대칭을 반영해야 도형이 상자 가운데에 오고 봉우리가 잘리지 않는다.
        x = u * 1.13
        y = -0.02 - v * 1.24          # v=-1(위) -> y=+1.22,  v=+1(아래) -> y=-1.26
        return ((x * x + y * y - 1.0) ** 3 - x * x * (y ** 3)) <= 0.0

    raise ValueError("알 수 없는 라벨: " + label)


# --------------------------------------------------------------------------- #
# 공개 API
# --------------------------------------------------------------------------- #

def rasterize(label: str, h: int = 32, w: int = 32, tile: int = 1,
              params: ShapeParams | None = None,
              rng: np.random.Generator | None = None) -> np.ndarray:
    """도형 하나를 h x w 타일 격자로 그린다. 반환: uint8 배열(0 = 빈 칸, 그 외 = tile)."""
    if label not in LABEL_INDEX:
        raise ValueError("알 수 없는 라벨: " + str(label))
    if not (4 <= h <= 256 and 4 <= w <= 256):
        raise ValueError("격자 크기 범위 밖: %dx%d" % (w, h))

    p = params or ShapeParams()
    rng = rng if rng is not None else np.random.default_rng()

    # 셀 중심을 정규 좌표로.
    ys = (np.arange(h) + 0.5) / h * 2.0 - 1.0
    xs = (np.arange(w) + 0.5) / w * 2.0 - 1.0
    v, u = np.meshgrid(ys, xs, indexing="ij")

    u = (u - p.cx) / max(p.scale * p.aspect, 1e-6)
    v = (v - p.cy) / max(p.scale, 1e-6)

    if p.rotation:
        c, s = math.cos(-p.rotation), math.sin(-p.rotation)
        u, v = u * c - v * s, u * s + v * c

    if p.jitter:
        # 경계를 흔들어 "손으로 그린" 느낌을 준다. 좌표를 흔들면 도형 종류와 무관하게 먹는다.
        u = u + rng.normal(0.0, p.jitter, u.shape)
        v = v + rng.normal(0.0, p.jitter, v.shape)

    mask = _filled_mask(label, u, v)

    if p.thickness > 0:
        mask = mask & ~erode(mask, p.thickness)

    if p.dropout > 0.0:
        mask = mask & (rng.random(mask.shape) >= p.dropout)

    return mask.astype(np.uint8) * np.uint8(tile)


def random_params(label: str, rng: np.random.Generator, augment: bool = True) -> ShapeParams:
    """랜덤 파라미터. **라벨별 정책**을 반영한다 (§3.2).

    뒤집은 하트는 하트가 아니고 회전한 삼각형은 삼각형으로 안 보인다 — 증강을 라벨과
    무관하게 일괄 적용하면 모델에게 거짓말을 가르치게 된다.

    augment 로 두 용도를 가른다. 같은 랜덤화를 쓰면 안 되기 때문이다:
      · True  (학습 데이터 증강): jitter·dropout으로 **일부러 흠집을 낸다.** 모델이
              깨끗한 도형에만 반응하지 않게 하려는 것이다.
      · False (생성/미리보기): 흠집 없이 크기·위치·회전·두께만 흔든다. 에디터 고스트에
              구멍 뚫린 도형을 띄우면 사용자는 그걸 "모델이 못 그린 것"으로 읽는다.
    """
    free_rot = label == "circle"                       # 원은 회전 불변 → 임의 각도
    quad_rot = label in ("square", "cross")            # 90도 단위만

    if free_rot:
        rotation = float(rng.uniform(0.0, 2.0 * math.pi))
    elif quad_rot:
        rotation = float(rng.integers(0, 4)) * (math.pi / 2.0)
    else:
        rotation = float(rng.normal(0.0, 0.05))        # 삼각형/별/하트는 미세 기울임만

    return ShapeParams(
        scale=float(rng.uniform(0.55, 0.95) if augment else rng.uniform(0.75, 0.95)),
        aspect=float(rng.uniform(0.85, 1.18) if augment else rng.uniform(0.92, 1.09)),
        cx=float(rng.uniform(-0.12, 0.12) if augment else rng.uniform(-0.05, 0.05)),
        cy=float(rng.uniform(-0.12, 0.12) if augment else rng.uniform(-0.05, 0.05)),
        rotation=rotation,
        thickness=int(rng.integers(0, 3)),             # 0=채움, 1~2=윤곽
        jitter=float(rng.uniform(0.0, 0.03)) if augment else 0.0,
        dropout=float(rng.uniform(0.0, 0.03)) if augment else 0.0,
    )


def to_ascii(grid: np.ndarray) -> str:
    """터미널에서 눈으로 확인하기 위한 표현. 검증에 쓴다."""
    return "\n".join("".join("##" if c else ". " for c in row) for row in grid)


def generate_dataset(out_dir: str, per_label: int, size: int, seed: int) -> None:
    """합성 데이터 생성 (P-015에서 본격 사용). 홀드아웃 라벨은 제외한다."""
    rng = np.random.default_rng(seed)
    for label in LABELS:
        if label in HOLDOUT_LABELS:
            print("  %-9s 건너뜀 (홀드아웃 — 손그림만 쓴다)" % label)
            continue
        stack = np.stack([
            rasterize(label, size, size, 1, random_params(label, rng), rng)
            for _ in range(per_label)
        ])
        os.makedirs(os.path.join(out_dir, label), exist_ok=True)
        path = os.path.join(out_dir, label, "%s_%dx%d.npy" % (label, per_label, size))
        np.save(path, stack)
        print("  %-9s %s -> %s" % (label, stack.shape, path))


def main() -> int:
    ap = argparse.ArgumentParser(description="절차적 도형 래스터라이저")
    ap.add_argument("--preview", metavar="LABEL", help="한 도형을 ASCII로 출력 (all = 전체)")
    ap.add_argument("--out", help="합성 데이터셋 출력 폴더")
    ap.add_argument("--per-label", type=int, default=2000)
    ap.add_argument("--size", type=int, default=32)
    ap.add_argument("--seed", type=int, default=20260830)
    ap.add_argument("--random", action="store_true", help="미리보기에 랜덤 파라미터 적용")
    args = ap.parse_args()

    print("seed=%d" % args.seed)

    if args.preview:
        rng = np.random.default_rng(args.seed)
        targets = LABELS if args.preview == "all" else [args.preview]
        for label in targets:
            p = random_params(label, rng) if args.random else None
            print("\n=== %s ===" % label)
            print(to_ascii(rasterize(label, args.size, args.size, 1, p, rng)))
        return 0

    if args.out:
        generate_dataset(args.out, args.per_label, args.size, args.seed)
        return 0

    ap.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
