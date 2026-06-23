#pragma once

#include "platform/IRenderDevice.h"
#include "platform/IWindow.h"
#include "platform/Input.h"

#include <chrono>

// 두 앱(게임/에디터) 호스트 루프의 공통 골격(Template Method 패턴).
// 불변부(타이밍·dt 스파이크 클램프·PollEvents·Present)는 여기서 한 번만 정의하고,
// 가변부(장면 전환·오버레이·전역 ESC 등)는 파생 클래스가 Frame() 하나로 구현한다.
// → ADR-009가 트레이드오프로 적은 "공통 호스트 루프 코드가 두 앱에 중복된다"를 상환한다.
//   dt 클램프 같은 손맛/안정성 상수가 단일 출처(이 파일)에만 존재하게 된다.
// 인터페이스(IWindow/IRenderDevice)에만 의존하며 SDL을 직접 모른다(ADR-006).
// 헤더온리(인라인) — vcxproj에 .cpp를 추가하지 않고 두 앱이 include만으로 공유한다.
namespace gs::core {

class HostLoop {
public:
    HostLoop(platform::IWindow& window, platform::IRenderDevice& renderer)
        : m_window(window), m_renderer(renderer) {}
    virtual ~HostLoop() = default;

    // 템플릿 메서드: 종료 전까지 [폴 → dt 계산·클램프 → Frame() → Present]를 반복한다.
    // 골격은 고정이고, 한 프레임의 의미는 Frame()이 결정한다.
    void Run() {
        using clock = std::chrono::steady_clock;
        auto prev = clock::now();

        while (!m_window.ShouldClose()) {
            m_window.PollEvents();

            const auto now = clock::now();
            float dt = std::chrono::duration<float>(now - prev).count();
            prev = now;
            if (dt > kMaxFrameDt) dt = kMaxFrameDt; // 스파이크 클램프(디버거 멈춤 등 큰 dt가 물리를 터뜨리는 것 방지)

            if (!Frame(m_window.GetInput(), dt)) break; // Frame이 화면을 그린다. false면 종료(이 프레임은 Present 안 함)
            m_renderer.Present();
        }
    }

protected:
    // 한 프레임의 가변 로직: 입력 처리·갱신·렌더(Clear~Draw)를 수행한다.
    // 반환값 false = 애플리케이션 종료. Present는 호출하지 않는다(기반 루프가 담당).
    virtual bool Frame(const platform::Input& in, float dt) = 0;

    platform::IWindow&       m_window;
    platform::IRenderDevice& m_renderer;

private:
    static constexpr float kMaxFrameDt = 0.05f; // 프레임당 최대 dt(초)
};

} // namespace gs::core
