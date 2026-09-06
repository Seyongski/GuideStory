"""도형 생성 추론 서버 (G0 스텁 → G2에서 실모델로 교체).

**이 파일의 목적은 관통이다.** 학습 모델이 아직 없으므로 tools/synth_shapes.py 의 절차적
래스터라이저로 격자를 만들어 돌려준다. C++ 에디터 ↔ 소켓 ↔ 파이썬 경로를 ML 위험 없이
먼저 검증한다 (docs/ai-roadmap.md G0).

G2에서는 _generate() 안쪽만 학습 모델 호출로 바꾼다. **C++ 을 건드리게 되면 여기서
계약을 잘못 그은 것이다.**

--------------------------------------------------------------------------------
와이어 포맷 — GuideStory/src/net/Framing 과 **같은 규약**을 쓴다.

    [헤더 6바이트][바디 N바이트]
    헤더: BodySize(uint32 LE) + Opcode(uint16 LE)   <- net::PacketHeader, #pragma pack(1)
    바디: UTF-8 JSON

프레이밍을 새로 설계하지 않고 재사용하는 이유: TCP 경계 문제를 이미 한 번 풀어놨고,
C++ 쪽(P-012)이 TryExtractPacket 을 그대로 쓸 수 있어야 하기 때문이다.

다만 **두 가지는 의도적으로 다르다**:
  · 옵코드 공간이 독립이다. 여기는 별도 엔드포인트(다른 포트)이므로 계정/채팅 옵코드와
    번호를 공유할 이유가 없다. 헤더 레이아웃만 같으면 프레이밍 코드는 공유된다.
  · 바디 상한이 1 MiB 다. net::kMaxBodySize 는 4096인데 그건 채팅 한 줄 기준이고,
    32x32 격자 JSON은 그걸 넘는다. 상한을 **검사한다는 원칙은 같고 값만 다르다.**

옵코드는 계정 서버와 같은 규칙을 따른다 — **항상 끝에 추가한다.**
--------------------------------------------------------------------------------
"""
from __future__ import annotations

import argparse
import json
import socket
import socketserver
import struct
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from tools import synth_shapes  # noqa: E402

PROTOCOL_VERSION = 1

# net::PacketHeader 와 동일한 레이아웃. '<' = 리틀엔디언, 패딩 없음.
HEADER = struct.Struct("<IH")
assert HEADER.size == 6, "헤더 크기가 net::PacketHeader(6바이트)와 어긋났다"

MAX_BODY = 1 << 20  # 1 MiB — 선언된 길이를 신뢰하기 전에 검사한다

# AI 채널 전용 옵코드. 항상 끝에 추가한다.
OP_GENERATE_REQ = 1
OP_GENERATE_ACK = 2
OP_PING_REQ = 3
OP_PING_ACK = 4

# 요청이 지정할 수 있는 격자 크기 한계. 클라이언트를 신뢰하지 않는다.
MIN_DIM, MAX_DIM = 4, 128


def encode_frame(opcode: int, payload: dict) -> bytes:
    """헤더와 바디를 **한 번에** 만들어 보낸다 — 따로 보내면 다른 스레드의 send가 끼어든다."""
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    if len(body) > MAX_BODY:
        body = json.dumps({"v": PROTOCOL_VERSION, "ok": False,
                           "error": "응답이 너무 큽니다"}).encode("utf-8")
    return HEADER.pack(len(body), opcode) + body


def try_extract(buf: bytearray):
    """버퍼 앞에서 완성된 프레임 하나를 꺼낸다.

    반환: (opcode, body_bytes) | None(더 필요) | 예외(상한 초과 -> 연결 종료)
    net::TryExtractPacket 과 같은 3단 판정이다.
    """
    if len(buf) < HEADER.size:
        return None
    body_size, opcode = HEADER.unpack_from(buf, 0)
    if body_size > MAX_BODY:
        raise ValueError("BodySize 상한 초과: %d" % body_size)
    total = HEADER.size + body_size
    if len(buf) < total:
        return None
    body = bytes(buf[HEADER.size:total])
    del buf[:total]
    return opcode, body


class ShapeService:
    """생성 로직. G2에서 이 클래스만 학습 모델 버전으로 교체한다."""

    def __init__(self, model_path: str | None = None):
        self.model_path = model_path
        if model_path:
            raise NotImplementedError(
                "학습 모델 로드는 G2(P-016)에서 구현한다. 지금은 --stub 으로 실행한다."
            )
        self.name = "stub-raster"

    def generate(self, req: dict) -> dict:
        """요청 dict -> 응답 dict. 실패도 예외가 아니라 응답이다."""
        label = req.get("label")
        if not isinstance(label, str) or label not in synth_shapes.LABEL_INDEX:
            # 모르는 라벨은 **추측하지 않고 거절한다** (ADR-012).
            return {"ok": False, "error": "알 수 없는 라벨: %r" % (label,)}

        try:
            w = int(req.get("w", 32))
            h = int(req.get("h", 32))
            tile = int(req.get("tile", 1))
            seed = int(req.get("seed", 0))
        except (TypeError, ValueError):
            return {"ok": False, "error": "요청 필드 형식 오류"}

        if not (MIN_DIM <= w <= MAX_DIM and MIN_DIM <= h <= MAX_DIM):
            return {"ok": False,
                    "error": "격자 크기 범위 밖(%d~%d): %dx%d" % (MIN_DIM, MAX_DIM, w, h)}
        if not (0 <= tile <= 255):
            return {"ok": False, "error": "타일 번호 범위 밖: %d" % tile}

        # seed 0 = "아무거나" — 실제로 쓴 값을 응답에 담아 같은 결과를 다시 뽑을 수 있게 한다.
        if seed == 0:
            seed = int.from_bytes(np.random.bytes(4), "little") or 1
        rng = np.random.default_rng(seed)

        t0 = time.perf_counter()
        # augment=False — 생성물에는 흠집(jitter/dropout)을 넣지 않는다. 학습 데이터와 용도가 다르다.
        params = synth_shapes.random_params(label, rng, augment=False)
        grid = synth_shapes.rasterize(label, h, w, tile, params, rng)
        elapsed_ms = (time.perf_counter() - t0) * 1000.0

        return {
            "ok": True,
            "model": self.name,
            "label": label,
            "seed": seed,
            "w": w,
            "h": h,
            # row-major 평탄화 — C++ ShapeResult.grid 와 같은 배치다.
            "grid": grid.reshape(-1).tolist(),
            "inference_ms": round(elapsed_ms, 3),
        }


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        peer = "%s:%d" % self.client_address
        print("[+] 접속 %s" % peer, flush=True)
        buf = bytearray()
        try:
            self.request.settimeout(300.0)
            while True:
                chunk = self.request.recv(65536)
                if not chunk:
                    break
                buf.extend(chunk)

                while True:
                    try:
                        frame = try_extract(buf)
                    except ValueError as e:
                        print("[!] %s 비정상 프레임: %s — 연결 종료" % (peer, e), flush=True)
                        return
                    if frame is None:
                        break
                    self._dispatch(peer, *frame)
        except socket.timeout:
            print("[-] %s 타임아웃" % peer, flush=True)
        except ConnectionError as e:
            print("[-] %s 연결 끊김: %s" % (peer, e), flush=True)
        finally:
            print("[-] 종료 %s" % peer, flush=True)

    def _dispatch(self, peer: str, opcode: int, body: bytes) -> None:
        service: ShapeService = self.server.service

        if opcode == OP_PING_REQ:
            self.request.sendall(encode_frame(OP_PING_ACK, {
                "v": PROTOCOL_VERSION, "ok": True, "model": service.name}))
            return

        if opcode != OP_GENERATE_REQ:
            self.request.sendall(encode_frame(OP_GENERATE_ACK, {
                "v": PROTOCOL_VERSION, "ok": False,
                "error": "알 수 없는 옵코드: %d" % opcode}))
            return

        try:
            req = json.loads(body.decode("utf-8"))
            if not isinstance(req, dict):
                raise ValueError("최상위가 객체가 아님")
        except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as e:
            self.request.sendall(encode_frame(OP_GENERATE_ACK, {
                "v": PROTOCOL_VERSION, "ok": False, "error": "JSON 파싱 실패: %s" % e}))
            return

        t0 = time.perf_counter()
        resp = service.generate(req)
        resp["v"] = PROTOCOL_VERSION
        total_ms = (time.perf_counter() - t0) * 1000.0

        if resp.get("ok"):
            print("[>] %s %s %dx%d seed=%d  %.2f ms" % (
                peer, resp["label"], resp["w"], resp["h"], resp["seed"], total_ms), flush=True)
        else:
            print("[>] %s 거절: %s" % (peer, resp.get("error")), flush=True)

        self.request.sendall(encode_frame(OP_GENERATE_ACK, resp))


class Server(socketserver.ThreadingTCPServer):
    # 접속당 스레드 1개 — 계정 서버(ADR-001)와 같은 모델. 에디터 한 명이 쓰는 도구라
    # 이 이상의 동시성 설계는 과하다.
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, addr, handler, service: ShapeService):
        self.service = service
        super().__init__(addr, handler)


def main() -> int:
    ap = argparse.ArgumentParser(description="GuideStory 도형 생성 추론 서버")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7788)
    ap.add_argument("--stub", action="store_true",
                    help="절차적 래스터라이저로 응답 (G0 기본값)")
    ap.add_argument("--model", help="학습된 TorchScript 모델 경로 (G2부터)")
    args = ap.parse_args()

    if args.model and args.stub:
        print("--stub 과 --model 은 함께 쓸 수 없습니다", file=sys.stderr)
        return 2

    service = ShapeService(args.model)

    with Server((args.host, args.port), Handler, service) as srv:
        print("도형 생성 서버 — %s  (%s:%d)" % (service.name, args.host, args.port), flush=True)
        print("라벨: %s" % ", ".join(synth_shapes.LABELS), flush=True)
        print("Ctrl+C 로 종료", flush=True)
        try:
            srv.serve_forever()
        except KeyboardInterrupt:
            print("\n종료합니다", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
