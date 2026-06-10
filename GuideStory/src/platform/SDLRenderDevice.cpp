#include "platform/SDLRenderDevice.h"

#include "platform/SDLWindow.h"

#include <SDL.h>

#include <stdexcept>
#include <string>

namespace gs::platform {

SDLRenderDevice::SDLRenderDevice(SDLWindow& window)
    : m_renderer(nullptr, &SDL_DestroyRenderer) // ADR-002: Custom Deleter
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
}

SDLRenderDevice::~SDLRenderDevice() = default; // RAII가 SDL_DestroyRenderer 호출

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

void SDLRenderDevice::Present() {
    SDL_RenderPresent(m_renderer.get());
}

} // namespace gs::platform
