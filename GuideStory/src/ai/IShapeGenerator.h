#pragma once

#include "ai/AiTypes.h"

// 도형 생성기 인터페이스. 구현은 세 가지가 예정돼 있고 에디터는 어느 것인지 모른다
// (ADR-006의 IRenderDevice/IWindow와 같은 격리 패턴):
//   NullShapeGenerator   — AI 미연결 폴백 (현재)
//   RemoteShapeGenerator — 소켓 + JSON으로 파이썬 추론 서버 질의 (P-012)
//   TorchShapeGenerator  — libtorch로 TorchScript 인프로세스 추론 (P-017)
// 마지막 둘을 같은 인터페이스 뒤에 두는 것이 ADR-007 증명 과제(IPC vs 인프로세스 지연 비교)의
// 전제다. 하나만 만들면 비교할 대상이 없다.
namespace gs::ai {

class IShapeGenerator {
public:
    virtual ~IShapeGenerator() = default;

    // 이 생성기를 실제로 쓸 수 있는가. false면 에디터는 AI 패널을 비활성으로 표시한다.
    // **AI가 없어도 에디터는 완전히 동작해야 한다** — AI는 부가 기능이다(ADR-015).
    virtual bool Available() const = 0;

    // **지금 당장** 요청을 받을 수 있는가. Available()이 "구성돼 있는가"라면 이건 "붙어 있는가"다.
    //   Null     : 언제나 false
    //   Remote   : 소켓이 연결돼 있는가 (서버를 껐다 켜면 값이 바뀐다)
    //   Torch    : 모델이 로드됐는가
    // 에디터는 이 값으로 "서버 꺼짐" 안내를 띄운다 — 생성을 시도해야만 알 수 있으면 늦다.
    virtual bool Ready() const { return Available(); }

    // 사람이 읽는 식별자("none", "remote 127.0.0.1:7788", "libtorch cvae_v1").
    // 상태 표시줄과 로그에 쓴다. 어느 경로로 생성됐는지 화면에서 바로 보여야
    // 스텁/실모델을 헷갈리지 않는다.
    virtual const char* Name() const = 0;

    // 생성 요청. **즉시 돌아온다.** 결과는 Poll()로 가져간다.
    // 생성이 수백 ms 걸려도 편집 프레임이 멈추면 안 되기 때문이다
    // (net::NetClient의 워커 스레드 + 사건 큐와 같은 모델).
    //
    // 처리 중에 다시 호출하면 **이전 요청을 버리고 새 것으로 교체한다.**
    // 사용자가 재생성(R)을 연타할 때 큐가 쌓이면 놓은 지 한참 지난 결과가 뒤늦게
    // 화면에 튀어나온다 — 마지막 요청만 의미가 있다.
    virtual void Request(const ShapeRequest& req) = 0;

    // 결과가 준비됐으면 out으로 옮기고 true. 없으면 false(out은 건드리지 않는다).
    // 매 프레임 호출한다. **실패도 결과다** — ok=false + error로 돌아오며 예외를 던지지 않는다.
    virtual bool Poll(ShapeResult& out) = 0;

    // 진행 중인 요청이 있는가(스피너 표시·버튼 비활성용).
    virtual bool Pending() const = 0;

    // 스레드 규약: Request()/Poll()/Pending()은 **에디터 스레드에서만** 호출한다.
    // 소켓·추론은 구현 내부의 워커가 담당하고, 그 경계를 넘는 것은 결과 큐뿐이다.
};

} // namespace gs::ai
