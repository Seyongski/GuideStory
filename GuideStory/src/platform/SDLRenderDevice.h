#pragma once

#include "platform/IRenderDevice.h"

#include <memory>
#include <string>

struct SDL_Renderer;
struct TTF_Font;

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
    void DrawText(const std::string& utf8, const math::Vector2D& topLeft,
                  float pixelHeight, const Color& color) override;
    math::Vector2D MeasureText(const std::string& utf8, float pixelHeight) const override;
    void Present() override;

private:
    // ADR-002: Custom Deleter로 SDL_DestroyRenderer / TTF_CloseFont 캡슐화.
    using RendererPtr = std::unique_ptr<SDL_Renderer, void (*)(SDL_Renderer*)>;
    using FontPtr     = std::unique_ptr<TTF_Font, void (*)(TTF_Font*)>;

    RendererPtr m_renderer;
    FontPtr     m_font;          // 기준 크기로 1회 로드, DrawText에서 스케일링.
    bool        m_ttfReady = false; // TTF_Init 성공 여부 → 소멸자에서 TTF_Quit 가드.
};

} // namespace gs::platform
