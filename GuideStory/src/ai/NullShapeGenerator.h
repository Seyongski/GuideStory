#pragma once

#include "ai/IShapeGenerator.h"

namespace gs::ai {

// AI가 연결되지 않았을 때의 폴백(널 객체 패턴).
//
// 이 구현이 존재하는 이유는 편의가 아니라 **계약**이다. 에디터는 생성기 포인터가 null인지
// 검사하지 않고, 항상 IShapeGenerator를 하나 갖는다. AI 서버가 없든, 주소 설정 파일이
// 없든, libtorch 없이 빌드했든 편집 기능은 그대로다 — ADR-015의 "에디터가 AI 없이
// 완결된다"를 코드로 보증하는 자리다.
//
// Request()를 조용히 무시하지 않고 **실패 결과 하나를 남긴다.** Available()을 확인하지 않고
// 요청한 호출자가 오지 않을 응답을 영원히 기다리는 것보다, 사유가 적힌 실패를 받는 편이 낫다.
class NullShapeGenerator final : public IShapeGenerator {
public:
    bool        Available() const override { return false; }
    const char* Name()      const override { return "none"; }

    void Request(const ShapeRequest&) override { m_pending = true; }

    bool Poll(ShapeResult& out) override {
        if (!m_pending) return false;
        m_pending = false;

        out       = ShapeResult{};
        out.ok    = false;
        out.error = "AI 생성기가 연결되지 않았습니다";
        out.model = "none";
        return true;
    }

    bool Pending() const override { return m_pending; }

private:
    bool m_pending = false;
};

} // namespace gs::ai
