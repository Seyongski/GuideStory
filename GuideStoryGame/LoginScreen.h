#pragma once

#include "Screen.h"

namespace gs::app {

// 로그인창(1차 placeholder). 추후 아이디/비밀번호 입력·계정 생성·찾기 UI가 들어갈 자리다.
// 지금은 아이디/비밀번호 칸을 시각적으로만 보여주고, Enter 또는 클릭 시 메인화면으로 진입한다.
// (ESC는 창 차원에서 애플리케이션을 종료한다.)
class LoginScreen final : public Screen {
public:
    SceneId Update(const platform::Input& in, float dt) override;
    void Render(platform::IRenderDevice& r) override;
};

} // namespace gs::app
