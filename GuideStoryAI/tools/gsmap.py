""".gsmap 파서/직렬화기 — C++ world::Map 의 파이썬 미러.

**학습 파이프라인 전체가 이 파일 하나에 걸려 있다.** 여기가 틀리면 나머지가 전부 조용히 틀린다.
그리고 언어가 달라 컴파일러가 잡아주지 못하므로, 왕복 테스트(--roundtrip)가 유일한 방어선이다.

--------------------------------------------------------------------------------
[바이트 동일 왕복을 위한 설계 — 이 파일에서 가장 중요한 부분]

C++ 쪽은 텍스트 모드 ofstream 으로 쓰므로 줄바꿈이 **CRLF** 이고, float 는 기본
ostream 서식(유효숫자 6자리 = printf "%g")으로 나간다:

    SPAWN 1280 440
    CAMERAVIEW 255.764 288.264 787.037 442.708

파이썬이 이 값을 다시 계산해서 쓰면 서식이 미묘하게 어긋나 왕복이 깨진다. 그래서
**우리가 고치지 않은 블록은 원문 그대로 다시 내보낸다.** 고친 블록만 규칙대로 재생성한다.

  · 우리가 소유(재생성)   : TILES, FOOTHOLDS, SPAWN, CONCEPT, AIGEN, SIZE/TILESIZE, 버전
  · 원문 보존             : PLAYERBOUNDS, CAMERAVIEW, BACKGROUND, BGSIZE, PORTALS,
                            OBJECTS, MOBS, 그리고 **모르는 키 전부**

마지막 항목이 C++ 파서의 "모르는 태그는 건너뛴다" 규칙(P-010)과 짝을 이룬다. 양쪽 다
모르는 것을 잃지 않으므로, 한쪽만 아는 필드가 생겨도 왕복에서 사라지지 않는다.
--------------------------------------------------------------------------------
"""
from __future__ import annotations

import argparse
import os
import sys
from dataclasses import dataclass, field

import numpy as np

FORMAT_VERSION = 3
EMPTY_TOKEN = "-"          # 공백 없는 "빈 값" 토큰 (C++ kEmptyTarget)

# 카운트 + 데이터 줄로 이루어진 섹션들. 헤더 줄의 숫자만큼 뒤에 줄이 따라온다.
COUNTED_SECTIONS = ("FOOTHOLDS", "PORTALS", "OBJECTS", "MOBS")


def fmt(v: float) -> str:
    """C++ 기본 ostream float 서식(유효숫자 6자리)과 같게. 1280.0 -> '1280'."""
    return "%g" % v


@dataclass
class Foothold:
    id: int = 0
    x1: float = 0.0
    y1: float = 0.0
    x2: float = 0.0
    y2: float = 0.0
    prev: int = 0
    next: int = 0

    def line(self) -> str:
        return " ".join((str(self.id), fmt(self.x1), fmt(self.y1),
                         fmt(self.x2), fmt(self.y2), str(self.prev), str(self.next)))


@dataclass
class _Block:
    """파일의 한 덩어리. dirty 면 재생성하고, 아니면 raw 를 그대로 내보낸다."""
    kind: str                      # 'scalar' | 'tiles' | 'counted' | 'end'
    key: str
    raw: list[str] = field(default_factory=list)
    dirty: bool = False


class GsMap:
    def __init__(self) -> None:
        self.version = FORMAT_VERSION
        self.tile_size = 32
        self.width = 0
        self.height = 0
        self.tiles = np.zeros((0, 0), dtype=np.uint8)
        self.concept = ""
        self.spawn = (0.0, 0.0)
        self.footholds: list[Foothold] = []
        self._blocks: list[_Block] = []
        self._newline = "\r\n"

    # --- 조회 ------------------------------------------------------------- #

    @property
    def label(self) -> str:
        """CONCEPT 값 = 학습 데이터의 정답 라벨. 없으면 빈 문자열."""
        return self.concept

    def _find(self, key: str) -> _Block | None:
        for b in self._blocks:
            if b.key == key:
                return b
        return None

    # --- 수정 (수정한 블록만 dirty 로 표시된다) ---------------------------- #

    def set_tiles(self, arr: np.ndarray) -> None:
        arr = np.asarray(arr, dtype=np.uint8)
        if arr.ndim != 2:
            raise ValueError("tiles 는 2차원이어야 한다: %r" % (arr.shape,))
        self.tiles = arr
        h, w = arr.shape
        if (w, h) != (self.width, self.height):
            self.width, self.height = w, h
            self._touch("SIZE")
        self._touch("TILES")

    def set_concept(self, value: str) -> None:
        self.concept = value
        self._touch("CONCEPT")

    def set_spawn(self, x: float, y: float) -> None:
        self.spawn = (float(x), float(y))
        self._touch("SPAWN")

    def set_footholds(self, footholds: list[Foothold]) -> None:
        self.footholds = list(footholds)
        self._touch("FOOTHOLDS")

    def set_aigen(self, model: str, label: str, seed: int, revision: int = 1) -> None:
        """생성 출처를 남긴다 — 같은 model/seed 로 언제든 다시 뽑을 수 있어야 한다."""
        self._aigen = "%s %s %d %d" % (model, label, seed, revision)
        self._touch("AIGEN")

    def _touch(self, key: str) -> None:
        b = self._find(key)
        if b is not None:
            b.dirty = True
            return
        # 없던 블록이면 TILES 앞에 새로 끼운다(C++ Serialize 의 순서와 같게).
        kind = "counted" if key in COUNTED_SECTIONS else "scalar"
        new = _Block(kind=kind, key=key, dirty=True)
        for i, blk in enumerate(self._blocks):
            if blk.key == "TILES":
                self._blocks.insert(i, new)
                return
        self._blocks.append(new)

    # --- 직렬화 ----------------------------------------------------------- #

    def _emit(self, b: _Block) -> list[str]:
        if not b.dirty:
            return list(b.raw)          # 원문 보존 — 서식 차이로 왕복이 깨지지 않게

        if b.key == "GSMAP":
            return ["GSMAP %d" % self.version]
        if b.key == "TILESIZE":
            return ["TILESIZE %d" % self.tile_size]
        if b.key == "SIZE":
            return ["SIZE %d %d" % (self.width, self.height)]
        if b.key == "SPAWN":
            return ["SPAWN %s %s" % (fmt(self.spawn[0]), fmt(self.spawn[1]))]
        if b.key == "CONCEPT":
            return ["CONCEPT %s" % (self.concept if self.concept else EMPTY_TOKEN)]
        if b.key == "AIGEN":
            return ["AIGEN %s" % getattr(self, "_aigen", EMPTY_TOKEN)]
        if b.key == "TILES":
            rows = [" ".join(str(int(v)) for v in row) for row in self.tiles]
            return ["TILES"] + rows
        if b.key == "FOOTHOLDS":
            return ["FOOTHOLDS %d" % len(self.footholds)] + [f.line() for f in self.footholds]

        raise ValueError("재생성 규칙이 없는 블록: %s" % b.key)

    def dumps(self) -> str:
        lines: list[str] = []
        for b in self._blocks:
            lines.extend(self._emit(b))
        return self._newline.join(lines) + self._newline

    def save(self, path: str) -> None:
        # newline="" 로 열어야 파이썬이 줄바꿈을 다시 변환하지 않는다(우리가 직접 넣는다).
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(self.dumps())


def load(path: str) -> GsMap:
    with open(path, "r", encoding="utf-8", newline="") as f:
        text = f.read()

    m = GsMap()
    m._newline = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(m._newline)
    if lines and lines[-1] == "":
        lines.pop()                     # 마지막 줄바꿈 뒤의 빈 조각

    if not lines:
        raise ValueError("빈 파일: %s" % path)

    head = lines[0].split()
    if len(head) < 2 or head[0] != "GSMAP":
        raise ValueError("맵 형식 오류: 헤더(GSMAP) — %s" % path)
    m.version = int(head[1])
    if m.version < 1:
        raise ValueError("맵 형식 오류: 버전 %d" % m.version)
    m._blocks.append(_Block("scalar", "GSMAP", [lines[0]]))

    i = 1
    have_size = have_tiles = False
    while i < len(lines):
        raw = lines[i]
        parts = raw.split()
        if not parts:
            i += 1
            continue
        key = parts[0]

        if key == "END":
            m._blocks.append(_Block("end", "END", [raw]))
            i += 1
            break

        if key == "TILES":
            if not have_size:
                raise ValueError("맵 형식 오류: TILES 앞에 SIZE 필요")
            body = lines[i + 1: i + 1 + m.height]
            if len(body) < m.height:
                raise ValueError("맵 형식 오류: 타일 데이터 부족")
            grid = np.zeros((m.height, m.width), dtype=np.uint8)
            for y, row in enumerate(body):
                vals = row.split()
                if len(vals) != m.width:
                    raise ValueError("맵 형식 오류: %d행 타일 수 %d != %d"
                                     % (y, len(vals), m.width))
                grid[y] = [int(v) for v in vals]
            m.tiles = grid
            have_tiles = True
            m._blocks.append(_Block("tiles", "TILES", [raw] + body))
            i += 1 + m.height
            continue

        if key in COUNTED_SECTIONS:
            count = int(parts[1]) if len(parts) > 1 else 0
            if count < 0:
                raise ValueError("맵 형식 오류: %s 개수 %d" % (key, count))
            body = lines[i + 1: i + 1 + count]
            if len(body) < count:
                raise ValueError("맵 형식 오류: %s 데이터 부족" % key)
            if key == "FOOTHOLDS":
                m.footholds = []
                for row in body:
                    t = row.split()
                    if len(t) < 7:
                        raise ValueError("맵 형식 오류: 풋홀드 데이터 부족")
                    m.footholds.append(Foothold(int(t[0]), float(t[1]), float(t[2]),
                                                float(t[3]), float(t[4]),
                                                int(t[5]), int(t[6])))
            m._blocks.append(_Block("counted", key, [raw] + body))
            i += 1 + count
            continue

        # 스칼라 헤더 키. 우리가 아는 것만 값을 뽑고, 나머지는 원문만 들고 있는다
        # (C++ 의 "모르는 태그는 건너뛴다"와 짝 — 모르는 것을 잃지 않는다).
        if key == "TILESIZE":
            m.tile_size = int(parts[1])
            if m.tile_size <= 0:
                raise ValueError("맵 형식 오류: TILESIZE")
        elif key == "SIZE":
            m.width, m.height = int(parts[1]), int(parts[2])
            if m.width <= 0 or m.height <= 0:
                raise ValueError("맵 형식 오류: SIZE")
            have_size = True
        elif key == "SPAWN":
            m.spawn = (float(parts[1]), float(parts[2]))
        elif key == "CONCEPT":
            v = parts[1] if len(parts) > 1 else EMPTY_TOKEN
            m.concept = "" if v == EMPTY_TOKEN else v
        elif key == "AIGEN":
            m._aigen = " ".join(parts[1:])

        m._blocks.append(_Block("scalar", key, [raw]))
        i += 1

    if not have_size or not have_tiles:
        raise ValueError("맵 형식 오류: SIZE/TILES 누락 — %s" % path)
    return m


def crop_normalize(tiles: np.ndarray, size: int = 32, keep_aspect: bool = False) -> np.ndarray:
    """타일 바운딩박스를 잘라 size x size 로 최근접 리샘플한다.

    기본은 **가로세로를 각각 늘려 정사각형에 맞춘다**(keep_aspect=False). 종횡비는
    ShapeParams.aspect 로 증강할 축이므로, 학습 입력에서는 정규화해 없애는 편이
    라벨과 형태의 대응을 깨끗하게 만든다. 비율을 지켜야 하는 실험을 위해
    keep_aspect=True 로 여백(0)을 채워 정사각형에 맞추는 경로도 둔다.
    """
    tiles = np.asarray(tiles, dtype=np.uint8)
    ys, xs = np.nonzero(tiles)
    if ys.size == 0:
        return np.zeros((size, size), dtype=np.uint8)

    crop = tiles[ys.min(): ys.max() + 1, xs.min(): xs.max() + 1]
    ch, cw = crop.shape

    if keep_aspect:
        side = max(ch, cw)
        square = np.zeros((side, side), dtype=np.uint8)
        oy, ox = (side - ch) // 2, (side - cw) // 2
        square[oy:oy + ch, ox:ox + cw] = crop
        crop, ch, cw = square, side, side

    # 최근접: 출력 칸의 중심을 입력 좌표로 되돌린다(가장자리 편향 없이).
    yi = np.clip(((np.arange(size) + 0.5) * ch / size).astype(int), 0, ch - 1)
    xi = np.clip(((np.arange(size) + 0.5) * cw / size).astype(int), 0, cw - 1)
    return crop[np.ix_(yi, xi)]


def to_ascii(tiles: np.ndarray) -> str:
    return "\n".join("".join("##" if c else ". " for c in row) for row in tiles)


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #

def _roundtrip(path: str) -> bool:
    """파싱 → 재직렬화가 원본과 **바이트 동일**한가. P-014 의 완료 조건."""
    with open(path, "rb") as f:
        original = f.read()
    produced = load(path).dumps().encode("utf-8")

    ok = original == produced
    name = os.path.basename(path)
    if ok:
        print("[ OK ]  %-28s %d bytes" % (name, len(original)))
        return True

    print("[FAIL]  %-28s %d -> %d bytes" % (name, len(original), len(produced)))
    a, b = original.split(b"\n"), produced.split(b"\n")
    for n, (x, y) in enumerate(zip(a, b), 1):
        if x != y:
            print("        %d행 원본: %r" % (n, x[:80]))
            print("        %d행 생성: %r" % (n, y[:80]))
            break
    if len(a) != len(b):
        print("        줄 수: %d -> %d" % (len(a), len(b)))
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=".gsmap 파서 (C++ world::Map 의 미러)")
    ap.add_argument("--roundtrip", nargs="+", metavar="MAP", help="바이트 동일 왕복 검증")
    ap.add_argument("--info", metavar="MAP", help="맵 요약 출력")
    ap.add_argument("--ascii", metavar="MAP", help="타일 격자를 ASCII로 출력")
    ap.add_argument("--normalize", metavar="MAP", help="바운딩박스 크롭 + 32x32 정규화 출력")
    args = ap.parse_args()

    if args.roundtrip:
        ok = all(_roundtrip(p) for p in args.roundtrip)
        print("\n%s" % ("전부 통과" if ok else "실패"))
        return 0 if ok else 1

    if args.info:
        m = load(args.info)
        print("버전      : %d" % m.version)
        print("타일 크기 : %d" % m.tile_size)
        print("크기      : %d x %d 칸" % (m.width, m.height))
        print("라벨      : %s" % (m.concept or "(없음)"))
        print("스폰      : %s, %s" % (fmt(m.spawn[0]), fmt(m.spawn[1])))
        print("풋홀드    : %d개" % len(m.footholds))
        print("채운 칸   : %d / %d" % (int(np.count_nonzero(m.tiles)), m.tiles.size))
        print("블록      : %s" % ", ".join(b.key for b in m._blocks))
        return 0

    if args.ascii:
        print(to_ascii(load(args.ascii).tiles))
        return 0

    if args.normalize:
        print(to_ascii(crop_normalize(load(args.normalize).tiles)))
        return 0

    ap.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
