#pragma once

#include "platform/IRenderDevice.h"

#include <memory>

struct SDL_Renderer;

namespace gs::platform {

class SDLWindow;

// IRenderDevice의 SDL2 구현. SDL_Renderer를 RAII로 관리한다 (ADR-002).
class SDLRenderDevice final : public IRenderDevice {
public:
    explicit SDLRenderDevice(SDLWindow& window);
    ~SDLRenderDevice() override;

    SDLRenderDevice(const SDLRenderDevice&) = delete;
    SDLRenderDevice& operator=(const SDLRenderDevice&) = delete;

    void Clear(const Color& color) override;
    void FillRect(const math::Rect& rect, const Color& color) override;
    void DrawRect(const math::Rect& rect, const Color& color) override;
    void DrawLine(const math::Vector2D& a, const math::Vector2D& b, const Color& color) override;
    void Present() override;

private:
    // ADR-002: Custom Deleter로 SDL_DestroyRenderer 캡슐화.
    using RendererPtr = std::unique_ptr<SDL_Renderer, void (*)(SDL_Renderer*)>;

    RendererPtr m_renderer;
};

} // namespace gs::platform
