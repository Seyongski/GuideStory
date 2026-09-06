"""추론 서버 프로브 — 서버가 살아 있고 규약대로 답하는지 확인하는 CLI.

P-012에서 C++ `RemoteShapeGenerator` 를 붙일 때 **어느 쪽이 틀렸는지** 가르는 도구다.
프로브가 되는데 에디터가 안 되면 C++ 잘못이고, 프로브도 안 되면 서버 잘못이다.

사용:
    python serve/ai_client_probe.py --label heart            # 한 번 요청하고 ASCII로 출력
    python serve/ai_client_probe.py --selftest               # 규약 전반을 자동 검증
"""
from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
import time

HEADER = struct.Struct("<IH")
OP_GENERATE_REQ, OP_GENERATE_ACK = 1, 2
OP_PING_REQ, OP_PING_ACK = 3, 4


def send_frame(sock: socket.socket, opcode: int, payload: dict) -> None:
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    sock.sendall(HEADER.pack(len(body), opcode) + body)


def recv_frame(sock: socket.socket, timeout: float = 10.0):
    """헤더 6바이트를 먼저 다 읽고, 그 길이만큼 바디를 읽는다."""
    sock.settimeout(timeout)

    def recv_exact(n: int) -> bytes:
        buf = b""
        while len(buf) < n:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("연결이 끊겼습니다")
            buf += chunk
        return buf

    body_size, opcode = HEADER.unpack(recv_exact(HEADER.size))
    return opcode, json.loads(recv_exact(body_size).decode("utf-8"))


def to_ascii(grid, w: int, h: int) -> str:
    return "\n".join(
        "".join("##" if grid[y * w + x] else ". " for x in range(w)) for y in range(h)
    )


def connect(host: str, port: int) -> socket.socket:
    s = socket.create_connection((host, port), timeout=5.0)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return s


def run_once(host: str, port: int, label: str, w: int, h: int, tile: int, seed: int) -> int:
    with connect(host, port) as s:
        t0 = time.perf_counter()
        send_frame(s, OP_GENERATE_REQ,
                   {"v": 1, "label": label, "w": w, "h": h, "tile": tile, "seed": seed})
        opcode, resp = recv_frame(s)
        rtt = (time.perf_counter() - t0) * 1000.0

    if not resp.get("ok"):
        print("거절됨: %s" % resp.get("error"))
        return 1

    print(to_ascii(resp["grid"], resp["w"], resp["h"]))
    print()
    print("model=%s  seed=%d" % (resp["model"], resp["seed"]))
    print("왕복 %.2f ms / 서버 내부 %.2f ms  ->  IPC 오버헤드 %.2f ms"
          % (rtt, resp["inference_ms"], rtt - resp["inference_ms"]))
    return 0


def selftest(host: str, port: int) -> int:
    """와이어 규약 전반을 확인한다. G0 통과 조건의 근거가 되는 검사들."""
    fails = 0

    def check(ok: bool, what: str, extra: str = "") -> None:
        nonlocal fails
        print("%s  %s%s" % ("[ OK ]" if ok else "[FAIL]", what, ("  — " + extra) if extra else ""))
        if not ok:
            fails += 1

    # 1. 핑
    with connect(host, port) as s:
        send_frame(s, OP_PING_REQ, {"v": 1})
        op, resp = recv_frame(s)
        check(op == OP_PING_ACK and resp.get("ok"), "핑 응답", resp.get("model", ""))

    # 2. 6개 라벨 전부 생성되고 격자 크기가 맞는가
    with connect(host, port) as s:
        for label in ["square", "circle", "triangle", "cross", "star", "heart"]:
            send_frame(s, OP_GENERATE_REQ, {"v": 1, "label": label, "w": 32, "h": 32,
                                            "tile": 3, "seed": 12345})
            op, resp = recv_frame(s)
            ok = (op == OP_GENERATE_ACK and resp.get("ok")
                  and len(resp["grid"]) == 32 * 32
                  and set(resp["grid"]) <= {0, 3}
                  and any(resp["grid"]))
            check(ok, "생성 %-9s" % label,
                  "채운 칸 %d" % sum(1 for c in resp.get("grid", []) if c))

    # 3. 같은 seed = 같은 결과 (재현성)
    with connect(host, port) as s:
        send_frame(s, OP_GENERATE_REQ, {"v": 1, "label": "star", "seed": 777})
        _, a = recv_frame(s)
        send_frame(s, OP_GENERATE_REQ, {"v": 1, "label": "star", "seed": 777})
        _, b = recv_frame(s)
        check(a["grid"] == b["grid"], "같은 seed -> 같은 격자 (재현성)")

    # 4. 다른 seed = 다른 결과 (변형 생성)
        send_frame(s, OP_GENERATE_REQ, {"v": 1, "label": "star", "seed": 778})
        _, c = recv_frame(s)
        check(a["grid"] != c["grid"], "다른 seed -> 다른 격자 (변형)")

    # 5. seed=0 이면 서버가 정하고, 그 값을 응답에 담는다
    with connect(host, port) as s:
        send_frame(s, OP_GENERATE_REQ, {"v": 1, "label": "circle", "seed": 0})
        _, r = recv_frame(s)
        check(r.get("ok") and r.get("seed", 0) != 0, "seed=0 -> 서버가 정한 seed 반환",
              "seed=%s" % r.get("seed"))

    # 6. 모르는 라벨은 추측하지 않고 거절 (ADR-012)
    with connect(host, port) as s:
        send_frame(s, OP_GENERATE_REQ, {"v": 1, "label": "우주정거장"})
        _, r = recv_frame(s)
        check(r.get("ok") is False and r.get("error"), "모르는 라벨 거절", r.get("error", ""))

    # 7. 범위 밖 크기 거절 (클라이언트를 신뢰하지 않는다)
    with connect(host, port) as s:
        send_frame(s, OP_GENERATE_REQ, {"v": 1, "label": "circle", "w": 9999, "h": 9999})
        _, r = recv_frame(s)
        check(r.get("ok") is False, "범위 밖 격자 크기 거절", r.get("error", ""))

    # 8. 깨진 JSON에도 서버가 죽지 않고 사유를 돌려준다
    with connect(host, port) as s:
        body = b"{ this is not json"
        s.sendall(HEADER.pack(len(body), OP_GENERATE_REQ) + body)
        _, r = recv_frame(s)
        check(r.get("ok") is False, "깨진 JSON 거절(서버 생존)")

    # 9. 프레임 분할 전송 — TCP 경계를 직접 자르는지 확인
    with connect(host, port) as s:
        body = json.dumps({"v": 1, "label": "cross", "seed": 5}).encode()
        packet = HEADER.pack(len(body), OP_GENERATE_REQ) + body
        for i in range(0, len(packet), 3):      # 3바이트씩 쪼개서 보낸다
            s.sendall(packet[i:i + 3])
            time.sleep(0.002)
        _, r = recv_frame(s)
        check(r.get("ok") is True, "분할 전송된 프레임 재조립")

    # 10. 한 번에 두 요청을 붙여 보내기 — 합쳐진 스트림을 둘로 자르는지
    with connect(host, port) as s:
        blob = b""
        for lb in ("square", "circle"):
            body = json.dumps({"v": 1, "label": lb, "seed": 9}).encode()
            blob += HEADER.pack(len(body), OP_GENERATE_REQ) + body
        s.sendall(blob)
        _, r1 = recv_frame(s)
        _, r2 = recv_frame(s)
        check(r1.get("label") == "square" and r2.get("label") == "circle",
              "합쳐진 두 프레임 분리")

    # 11. 알 수 없는 옵코드
    with connect(host, port) as s:
        send_frame(s, 9999, {"v": 1})
        _, r = recv_frame(s)
        check(r.get("ok") is False, "알 수 없는 옵코드 거절", r.get("error", ""))

    # 12. 지연 측정 — 왕복이 사람이 기다릴 수준인가
    with connect(host, port) as s:
        times = []
        for _ in range(20):
            t0 = time.perf_counter()
            send_frame(s, OP_GENERATE_REQ, {"v": 1, "label": "heart", "seed": 0})
            recv_frame(s)
            times.append((time.perf_counter() - t0) * 1000.0)
        avg = sum(times) / len(times)
        check(avg < 50.0, "왕복 지연 20회 평균 < 50ms",
              "평균 %.2f ms / 최대 %.2f ms" % (avg, max(times)))

    print()
    print("전부 통과" if not fails else "실패 %d건" % fails)
    return 1 if fails else 0


def main() -> int:
    ap = argparse.ArgumentParser(description="도형 생성 서버 프로브")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7788)
    ap.add_argument("--label", default="heart")
    ap.add_argument("--w", type=int, default=32)
    ap.add_argument("--h", type=int, default=32)
    ap.add_argument("--tile", type=int, default=1)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--selftest", action="store_true", help="와이어 규약 전반 검증")
    args = ap.parse_args()

    try:
        if args.selftest:
            return selftest(args.host, args.port)
        return run_once(args.host, args.port, args.label, args.w, args.h, args.tile, args.seed)
    except (ConnectionError, socket.timeout, OSError) as e:
        print("서버에 연결할 수 없습니다 (%s:%d): %s" % (args.host, args.port, e),
              file=sys.stderr)
        print("먼저 실행하세요:  python serve/ai_server.py --stub", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
