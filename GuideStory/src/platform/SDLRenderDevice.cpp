#include "platform/SDLRenderDevice.h"

#include "platform/SDLWindow.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>
#include <stdexcept>
#include <string>

namespace gs::platform {

namespace {
// 폰트는 한 번만 이 크기로 로드하고, DrawText에서 텍스처를 요청 높이로 스케일링한다.
// (메뉴/HUD 수준에서는 텍스처 스케일링으로 충분 — 매 크기마다 폰트를 새로 여는 비용 회피.)
constexpr int kBaseFontPx = 48;

// 한글을 지원하는 시스템 폰트 후보(Windows 기본 탑재). 첫 성공을 사용한다.
// 번들 폰트를 추가하려면 이 목록 앞에 상대 경로를 넣으면 된다.
const char* const kFontCandidates[] = {
    "C:/Windows/Fonts/malgun.ttf",   // 맑은 고딕
    "C:/Windows/Fonts/gulim.ttc",    // 굴림
    "C:/Windows/Fonts/batang.ttc",   // 바탕
};
} // namespace

SDLRenderDevice::SDLRenderDevice(SDLWindow& window)
    : m_renderer(nullptr, &SDL_DestroyRenderer), // ADR-002: Custom Deleter
      m_font(nullptr, &TTF_CloseFont)
{
    SDL_Renderer* raw = SDL_CreateRenderer(
        window.Native(), -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (raw == nullptr) {
        throw std::runtime_error(std::string("SDL_CreateRenderer 실패: ") + SDL_GetError());
    }

    m_renderer.reset(raw);

    // 알파 블렌딩 활성화 — 반투명 디버그 오버레이/그리드용.
    SDL_SetRenderDrawBlendMode(m_renderer.get(), SDL_BLENDMODE_BLEND);

    // 텍스트 렌더링 초기화. 실패해도 게임은 계속 동작한다(텍스트만 미표시).
    if (TTF_Init() == 0) {
        m_ttfReady = true;
        for (const char* path : kFontCandidates) {
            if (TTF_Font* f = TTF_OpenFont(path, kBaseFontPx)) {
                m_font.reset(f);
                break;
            }
        }
        if (!m_font) {
            std::fprintf(stderr, "폰트 로드 실패 — 텍스트가 표시되지 않습니다 (%s)\n", TTF_GetError());
        }
    } else {
        std::fprintf(stderr, "TTF_Init 실패: %s\n", TTF_GetError());
    }
}

SDLRenderDevice::~SDLRenderDevice() {
    m_font.reset();                 // TTF_Quit 전에 폰트 핸들을 먼저 닫는다.
    if (m_ttfReady) TTF_Quit();
    // m_renderer는 RAII가 SDL_DestroyRenderer로 해제.
}

void SDLRenderDevice::Clear(const Color& color) {
    SDL_SetRenderDrawColor(m_renderer.get(), color.r, color.g, color.b, color.a);
    SDL_RenderClear(m_renderer.get());
}

void SDLRenderDevice::FillRect(const math::Rect& rect, const Color& color) {
    SDL_SetRenderDrawColor(m_renderer.get(), color.r, color.g, color.b, color.a);
    const SDL_FRect r{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderFillRectF(m_renderer.get(), &r);
}

void SDLRenderDevice::DrawRect(const math::Rect& rect, const Color& color) {
    SDL_SetRenderDrawColor(m_renderer.get(), color.r, color.g, color.b, color.a);
    const SDL_FRect r{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderDrawRectF(m_renderer.get(), &r);
}

void SDLRenderDevice::DrawLine(const math::Vector2D& a, const math::Vector2D& b, const Color& color) {
    SDL_SetRenderDrawColor(m_renderer.get(), color.r, color.g, color.b, color.a);
    SDL_RenderDrawLineF(m_renderer.get(), a.x, a.y, b.x, b.y);
}

void SDLRenderDevice::DrawText(const std::string& utf8, const math::Vector2D& topLeft,
                               float pixelHeight, const Color& color) {
    if (!m_font || utf8.empty()) return;

    const SDL_Color c{color.r, color.g, color.b, color.a};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font.get(), utf8.c_str(), c);
    if (!surf) return;

    if (SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer.get(), surf)) {
        const float scale = (surf->h > 0) ? pixelHeight / static_cast<float>(surf->h) : 1.0f;
        const SDL_FRect dst{topLeft.x, topLeft.y,
                            static_cast<float>(surf->w) * scale,
                            static_cast<float>(surf->h) * scale};
        SDL_RenderCopyF(m_renderer.get(), tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

math::Vector2D SDLRenderDevice::MeasureText(const std::string& utf8, float pixelHeight) const {
    if (!m_font || utf8.empty()) return {0.0f, 0.0f};

    int w = 0, h = 0;
    if (TTF_SizeUTF8(m_font.get(), utf8.c_str(), &w, &h) != 0 || h <= 0) {
        return {0.0f, 0.0f};
    }
    const float scale = pixelHeight / static_cast<float>(h);
    return {static_cast<float>(w) * scale, pixelHeight};
}

void SDLRenderDevice::Present() {
    SDL_RenderPresent(m_renderer.get());
}

} // namespace gs::platform
