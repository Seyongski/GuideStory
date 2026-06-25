#pragma once

#include "platform/IRenderDevice.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;
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
    TextureId LoadTexture(const std::string& path) override;
    void DrawTexture(TextureId tex, const math::Rect& dst) override;
    math::Vector2D TextureSize(TextureId tex) const override;
    void Present() override;

private:
    // ADR-002: Custom Deleter로 SDL_DestroyRenderer / TTF_CloseFont / SDL_DestroyTexture 캡슐화.
    using RendererPtr = std::unique_ptr<SDL_Renderer, void (*)(SDL_Renderer*)>;
    using FontPtr     = std::unique_ptr<TTF_Font, void (*)(TTF_Font*)>;
    using TexturePtr  = std::unique_ptr<SDL_Texture, void (*)(SDL_Texture*)>;

    // 렌더한 텍스트 한 줄을 캐시(매 프레임 서피스/텍스처 재생성 회피).
    struct CachedText {
        TexturePtr texture;
        int        w = 0; // 기준 폰트 크기로 렌더된 픽셀 크기(DrawText에서 스케일)
        int        h = 0;
    };

    // 로드한 이미지. 정지 이미지는 프레임 1개, 애니메이션 GIF는 여러 프레임 + 프레임별 지속(ms).
    // DrawTexture가 벽시계 시간(SDL_GetTicks)으로 현재 프레임을 골라 그린다 → 호출측은 시간을 몰라도 된다.
    struct LoadedImage {
        std::vector<TexturePtr> frames;   // ≥1개. 1개면 정지.
        std::vector<int>        delaysMs; // frames와 같은 길이(프레임 지속).
        int                     totalMs = 0; // 전체 사이클 길이(0이면 정지 취급).
        int                     w = 0, h = 0; // 원본 픽셀 크기(프레임 공통).
    };

    RendererPtr m_renderer;
    FontPtr     m_font;          // 기준 크기로 1회 로드, DrawText에서 스케일링.
    bool        m_ttfReady = false; // TTF_Init 성공 여부 → 소멸자에서 TTF_Quit 가드.
    bool        m_imgReady = false; // IMG_Init 성공 여부 → 소멸자에서 IMG_Quit 가드.

    // 텍스처/텍스트 캐시. m_renderer보다 뒤에 선언해 소멸자에서 렌더러보다 먼저 해제되도록
    // 한다(SDL 텍스처는 렌더러에 종속). 키: 이미지=경로, 텍스트=색+문자열.
    std::vector<LoadedImage>                   m_textures;
    std::unordered_map<std::string, TextureId> m_textureCache;
    std::unordered_map<std::string, CachedText> m_textCache;
};

} // namespace gs::platform
