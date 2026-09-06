"""데이터셋 빌더 — raw(.gsmap 손그림) / synth(절차 생성)를 학습 텐서로 만든다.

출력: data/dataset/{synth,raw}.npz  +  같은 이름의 .json(재현 정보)
  x : (N, 32, 32) uint8  — 0/1 이진 격자. **타일 번호는 버린다**
  y : (N,)        int64  — LABELS 인덱스

[왜 타일 번호를 버리는가]
  모델이 배우는 것은 **형태**이지 색이 아니다. 같은 하트를 흙으로 그리든 물로 그리든
  라벨은 하트다. 타일 번호는 생성 시점에 사용자가 고르는 값이라(ShapeRequest.tile)
  학습 입력에 넣으면 "물로 그린 하트"와 "흙으로 그린 하트"를 다른 것으로 배운다.

[증강이 두 곳에 있는 이유 — 헷갈리기 쉬운 지점]
  · 합성(synth): 생성 시점에 synth_shapes.random_params(augment=True) 가 크기·비율·회전·
    두께·흔들림·드롭아웃을 랜덤화한다. 2,000장이 이미 서로 다르므로 여기서 더 늘리지 않는다.
  · 손그림(raw): 40장뿐이라 **격자 수준 증강**으로 수천 장을 만든다. 평행이동/반전/회전/
    스케일/두께/드롭아웃을 이 파일이 적용한다.
  두 경로가 겹치지 않게 --synth-aug 기본값이 1인 이유가 이것이다.

[라벨별 증강 정책 — docs/ai-shape-synthesis.md §3.2]
  뒤집은 하트는 하트가 아니고 회전한 삼각형은 삼각형으로 안 보인다. 증강을 라벨과
  무관하게 일괄 적용하면 **모델에게 거짓말을 가르치게 된다.**
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from dataclasses import dataclass

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools import gsmap, synth_shapes  # noqa: E402

LABELS = synth_shapes.LABELS
LABEL_INDEX = synth_shapes.LABEL_INDEX
GRID = 32


@dataclass(frozen=True)
class AugPolicy:
    """이 라벨에 허용되는 기하 변환. 문서 §3.2 표가 이 자료구조의 단일 출처다."""
    hflip: bool     # 좌우 반전
    vflip: bool     # 상하 반전
    rot90: bool     # 90도 단위 회전


AUG_POLICY = {
    "square":   AugPolicy(hflip=True, vflip=True,  rot90=True),
    "circle":   AugPolicy(hflip=True, vflip=True,  rot90=True),
    "triangle": AugPolicy(hflip=True, vflip=False, rot90=False),  # 뒤집으면 삼각형으로 안 보인다
    "cross":    AugPolicy(hflip=True, vflip=True,  rot90=True),
    "star":     AugPolicy(hflip=True, vflip=True,  rot90=False),
    "heart":    AugPolicy(hflip=True, vflip=False, rot90=False),  # 뒤집은 하트는 하트가 아니다
}
assert set(AUG_POLICY) == set(LABELS), "AUG_POLICY 가 LABELS 와 어긋났다"


# --------------------------------------------------------------------------- #
# 격자 증강 (손그림용)
# --------------------------------------------------------------------------- #

def _translate(g: np.ndarray, dy: int, dx: int) -> np.ndarray:
    return synth_shapes.shift(g.astype(bool), dy, dx).astype(np.uint8)


def _rescale(g: np.ndarray, factor: float) -> np.ndarray:
    """도형만 확대/축소해 격자 가운데 다시 놓는다(배경 여백은 유지)."""
    ys, xs = np.nonzero(g)
    if ys.size == 0:
        return g
    crop = g[ys.min(): ys.max() + 1, xs.min(): xs.max() + 1]
    ch, cw = crop.shape
    nh = max(1, min(GRID, int(round(ch * factor))))
    nw = max(1, min(GRID, int(round(cw * factor))))
    yi = np.clip(((np.arange(nh) + 0.5) * ch / nh).astype(int), 0, ch - 1)
    xi = np.clip(((np.arange(nw) + 0.5) * cw / nw).astype(int), 0, cw - 1)
    scaled = crop[np.ix_(yi, xi)]

    out = np.zeros((GRID, GRID), dtype=np.uint8)
    oy, ox = (GRID - nh) // 2, (GRID - nw) // 2
    out[oy:oy + nh, ox:ox + nw] = scaled
    return out


def _thickness(g: np.ndarray, delta: int) -> np.ndarray:
    if delta == 0:
        return g
    m = g.astype(bool)
    m = synth_shapes.dilate(m, delta) if delta > 0 else synth_shapes.erode(m, -delta)
    return m.astype(np.uint8)


def geometric(g: np.ndarray, label: str, rng: np.random.Generator) -> np.ndarray:
    """**라벨 정책에 묶인** 변환만 적용한다(반전·90도 회전).

    크기·위치·두께 변형과 분리한 이유는 이것만 정책 위반 여부를 단정할 수 있기 때문이다 —
    "하트는 절대 상하로 뒤집히지 않는다"를 이 함수 하나로 검증할 수 있다.
    """
    pol = AUG_POLICY[label]
    out = g
    if pol.rot90:
        out = np.rot90(out, int(rng.integers(0, 4)))
    if pol.hflip and rng.random() < 0.5:
        out = np.fliplr(out)
    if pol.vflip and rng.random() < 0.5:
        out = np.flipud(out)
    return np.ascontiguousarray(out)


def augment(g: np.ndarray, label: str, rng: np.random.Generator) -> np.ndarray:
    """라벨 정책을 지키며 격자 하나를 변형한다."""
    out = geometric(g, label, rng)
    out = _rescale(out, float(rng.uniform(0.7, 1.3)))
    out = _translate(out, int(rng.integers(-4, 5)), int(rng.integers(-4, 5)))
    out = _thickness(out, int(rng.integers(-1, 2)))
    if rng.random() < 0.5:                       # 셀 드롭아웃 3%
        out = (out.astype(bool) & (rng.random(out.shape) >= 0.03)).astype(np.uint8)

    # 변형 끝에 도형이 사라졌으면(과한 침식/이동) 원본을 돌려준다 — 빈 격자는 학습에 해롭다.
    return out if out.any() else g


# --------------------------------------------------------------------------- #
# 수집
# --------------------------------------------------------------------------- #

def load_raw(raw_dir: str) -> tuple[list[np.ndarray], list[int], list[str]]:
    """data/raw/<label>/*.gsmap → 정규화된 32x32 이진 격자."""
    grids: list[np.ndarray] = []
    ys: list[int] = []
    warnings: list[str] = []

    if not os.path.isdir(raw_dir):
        return grids, ys, ["raw 폴더가 없다: " + raw_dir]

    for label in LABELS:
        folder = os.path.join(raw_dir, label)
        if not os.path.isdir(folder):
            continue
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".gsmap"):
                continue
            path = os.path.join(folder, name)
            try:
                m = gsmap.load(path)
            except Exception as e:                     # 한 장이 깨져도 나머지는 살린다
                warnings.append("%s 로드 실패: %s" % (name, e))
                continue

            # 폴더와 CONCEPT 가 다르면 **라벨 잡음**이다. 조용히 넘기지 않는다.
            if m.concept and m.concept != label:
                warnings.append("%s: 폴더=%s 인데 CONCEPT=%s — 건너뜀"
                                % (name, label, m.concept))
                continue
            if not m.concept:
                warnings.append("%s: CONCEPT 없음 — 폴더명(%s)으로 라벨링" % (name, label))

            g = gsmap.crop_normalize(m.tiles, GRID)
            if not g.any():
                warnings.append("%s: 타일이 하나도 없다 — 건너뜀" % name)
                continue
            grids.append((g > 0).astype(np.uint8))     # 타일 번호는 버린다
            ys.append(LABEL_INDEX[label])

    return grids, ys, warnings


def load_synth(synth_dir: str) -> tuple[list[np.ndarray], list[int], list[str]]:
    """data/synth/<label>/*.npy → 이진 격자."""
    grids: list[np.ndarray] = []
    ys: list[int] = []
    warnings: list[str] = []

    if not os.path.isdir(synth_dir):
        return grids, ys, ["synth 폴더가 없다: " + synth_dir]

    for label in LABELS:
        folder = os.path.join(synth_dir, label)
        if not os.path.isdir(folder):
            continue
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".npy"):
                continue
            stack = np.load(os.path.join(folder, name))
            if stack.ndim == 2:
                stack = stack[None, ...]
            for g in stack:
                if g.shape != (GRID, GRID):
                    g = gsmap.crop_normalize(g, GRID)
                grids.append((g > 0).astype(np.uint8))
                ys.append(LABEL_INDEX[label])

    return grids, ys, warnings


def build(grids, ys, multiplier: int, rng: np.random.Generator):
    """원본 + 증강본을 쌓는다. multiplier=1 이면 증강 없이 원본만."""
    xs_out: list[np.ndarray] = []
    ys_out: list[int] = []
    for g, y in zip(grids, ys):
        xs_out.append(g)
        ys_out.append(y)
        for _ in range(max(0, multiplier - 1)):
            xs_out.append(augment(g, LABELS[y], rng))
            ys_out.append(y)
    if not xs_out:
        return np.zeros((0, GRID, GRID), np.uint8), np.zeros((0,), np.int64)
    return np.stack(xs_out).astype(np.uint8), np.asarray(ys_out, dtype=np.int64)


def _git_commit() -> str:
    try:
        return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"],
                                       stderr=subprocess.DEVNULL,
                                       cwd=os.path.dirname(os.path.abspath(__file__))
                                       ).decode().strip()
    except Exception:
        return "(git 정보 없음)"


def save_dataset(path_npz: str, x: np.ndarray, y: np.ndarray, meta: dict) -> None:
    os.makedirs(os.path.dirname(path_npz), exist_ok=True)
    np.savez_compressed(path_npz, x=x, y=y, labels=np.array(LABELS))

    # 재현 정보는 별도 JSON — §7 재현성 규칙(seed, 데이터셋 해시, git 커밋).
    digest = hashlib.sha256(x.tobytes() + y.tobytes()).hexdigest()[:16]
    meta = dict(meta)
    meta.update({
        "count": int(x.shape[0]),
        "grid": GRID,
        "labels": LABELS,
        "per_label": {LABELS[i]: int((y == i).sum()) for i in range(len(LABELS))},
        "sha256_16": digest,
        "git": _git_commit(),
    })
    with open(os.path.splitext(path_npz)[0] + ".json", "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=2)


def report(name: str, x: np.ndarray, y: np.ndarray, warnings: list[str]) -> None:
    print("\n=== %s ===" % name)
    if x.shape[0] == 0:
        print("  (비어 있음)")
    else:
        print("  %d장  %dx%d" % (x.shape[0], x.shape[1], x.shape[2]))
        for i, label in enumerate(LABELS):
            n = int((y == i).sum())
            if n == 0:
                continue
            fill = x[y == i].mean() * 100.0
            print("    %-9s %6d장   평균 채움 %.1f%%" % (label, n, fill))
    for w in warnings[:10]:
        print("  ⚠ %s" % w)
    if len(warnings) > 10:
        print("  ⚠ ... 외 %d건" % (len(warnings) - 10))


def montage(path_png: str, x: np.ndarray, y: np.ndarray, cols: int, seed: int) -> None:
    """라벨별 표본을 한 장의 PNG로. **증강이 라벨을 망가뜨리지 않았는지 눈으로 확인**하는 용도다.

    지표만 보면 "정책은 지켰는데 도형이 뭉개진" 데이터를 놓친다. 학습을 돌리기 전에
    한 번 보는 것이 가장 싼 검증이다.
    """
    import matplotlib
    matplotlib.use("Agg")                      # 화면 없이 파일로만
    import matplotlib.pyplot as plt

    rng = np.random.default_rng(seed)
    rows = [i for i in range(len(LABELS)) if int((y == i).sum()) > 0]
    if not rows:
        return

    fig, axes = plt.subplots(len(rows), cols, figsize=(cols * 1.1, len(rows) * 1.25))
    axes = np.atleast_2d(axes)
    for r, li in enumerate(rows):
        idx = np.nonzero(y == li)[0]
        pick = rng.choice(idx, size=min(cols, idx.size), replace=False)
        for c in range(cols):
            ax = axes[r, c]
            ax.set_xticks([]); ax.set_yticks([])
            if c < pick.size:
                ax.imshow(x[pick[c]], cmap="binary", vmin=0, vmax=1, interpolation="nearest")
            else:
                ax.axis("off")
            if c == 0:
                ax.set_ylabel(LABELS[li], fontsize=9, rotation=0, ha="right", va="center")
    fig.suptitle("dataset samples (n=%d)" % x.shape[0], fontsize=10)
    fig.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(path_png)), exist_ok=True)
    fig.savefig(path_png, dpi=130)
    plt.close(fig)
    print("미리보기: %s" % os.path.abspath(path_png))


def main() -> int:
    ap = argparse.ArgumentParser(description="학습 데이터셋 빌더")
    ap.add_argument("--raw", default="data/raw", help="손그림 .gsmap 폴더")
    ap.add_argument("--synth", default="data/synth", help="절차 생성 .npy 폴더")
    ap.add_argument("--out", default="data/dataset", help="출력 폴더")
    ap.add_argument("--raw-aug", type=int, default=40,
                    help="손그림 1장당 총 장수(원본 포함). 40장 -> 수천 장")
    ap.add_argument("--synth-aug", type=int, default=1,
                    help="합성은 생성 시점에 이미 랜덤화됐다 — 기본 1(증강 없음)")
    ap.add_argument("--seed", type=int, default=20260830)
    ap.add_argument("--preview", action="store_true", help="라벨당 한 장을 ASCII로 출력")
    ap.add_argument("--montage", metavar="PNG", help="라벨별 표본을 PNG 한 장으로 (눈으로 확인)")
    ap.add_argument("--montage-cols", type=int, default=10)
    args = ap.parse_args()

    print("seed=%d" % args.seed)
    rng = np.random.default_rng(args.seed)

    common = {"seed": args.seed, "raw_aug": args.raw_aug, "synth_aug": args.synth_aug}

    sg, sy, sw = load_synth(args.synth)
    sx, syy = build(sg, sy, args.synth_aug, rng)
    report("synth (사전학습용)", sx, syy, sw)
    save_dataset(os.path.join(args.out, "synth.npz"), sx, syy,
                 dict(common, source="synth", dir=args.synth))

    rg, ry, rw = load_raw(args.raw)
    rx, ryy = build(rg, ry, args.raw_aug, rng)
    report("raw (파인튜닝용)", rx, ryy, rw)
    save_dataset(os.path.join(args.out, "raw.npz"), rx, ryy,
                 dict(common, source="raw", dir=args.raw, originals=len(rg)))

    if rx.shape[0] == 0:
        print("\n※ 손그림이 아직 없다. 에디터로 그려 data/raw/<label>/ 에 저장하면"
              "\n   이 명령이 파인튜닝용 데이터셋을 만든다 (라벨당 15장 이상 목표).")

    # 하트 홀드아웃이 지켜지는지 확인 — 이게 깨지면 §3.3 증명 설계가 무너진다.
    heart = LABEL_INDEX["heart"]
    if int((syy == heart).sum()) > 0:
        print("\n⚠ 합성 데이터에 하트가 섞였다 — 홀드아웃 설계 위반(§3.3)")
        return 1
    print("\n하트 홀드아웃 유지: 합성 0장 / 손그림 %d장" % int((ryy == heart).sum()))

    if args.montage:
        # 손그림이 있으면 두 출처를 함께 보여준다(합성만 예쁜지 손그림이 묻혔는지 한눈에).
        mx = np.concatenate([sx, rx]) if rx.shape[0] else sx
        my = np.concatenate([syy, ryy]) if rx.shape[0] else syy
        montage(args.montage, mx, my, args.montage_cols, args.seed)

    if args.preview:
        for i, label in enumerate(LABELS):
            pool = sx if int((syy == i).sum()) else rx
            py = syy if int((syy == i).sum()) else ryy
            sel = np.nonzero(py == i)[0]
            if sel.size == 0:
                continue
            print("\n--- %s ---" % label)
            print(synth_shapes.to_ascii(pool[sel[0]]))

    print("\n출력: %s" % os.path.abspath(args.out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
