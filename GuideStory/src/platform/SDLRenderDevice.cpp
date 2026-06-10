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
}

SDLRenderDevice::~SDLRenderDevice() = default; // RAII가 SDL_DestroyRenderer 호출

void SDLRenderDevice::Clear(const Color& color) {
    SDL_SetRenderDrawColor(m_renderer.get(), color.r, color.g, color.b, color.a);
    SDL_RenderClear(m_renderer.get());
}

void SDLRenderDevice::Present() {
    SDL_RenderPresent(m_renderer.get());
}

} // namespace gs::platform
