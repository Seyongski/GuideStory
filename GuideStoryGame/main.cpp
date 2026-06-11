// GuideStoryGame — 런타임(플레이) 실행파일의 진입점 / 합성 루트(Composition Root).
// 여기서만 구체 SDL 구현(SDLWindow/SDLRenderDevice)을 생성하고, 게임 로직에는 인터페이스로 주입한다.
// SDL은 SDL_main으로 main을 재정의하므로 인자 시그니처를 맞춘다.

#include "App.h"

#include "platform/SDLRenderDevice.h"
#include "platform/SDLWindow.h"

#include <SDL.h>

#include <cstdio>
#include <exception>

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        gs::platform::SDLWindow window("GuideStory", 1280, 720);
        gs::platform::SDLRenderDevice renderer(window);

        gs::app::App app(window, renderer);
        app.Run();
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "치명적 오류: %s\n", e.what());
        return 1;
    }
    return 0;
}
