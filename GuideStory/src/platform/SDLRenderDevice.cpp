#include "platform/SDLRenderDevice.h"

#include "platform/SDLWindow.h"

#include <SDL.h>
#include <SDL_image.h>
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

    // 이미지(PNG) 로딩 초기화. 실패해도 게임은 계속 동작한다(이미지만 미표시).
    if (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) {
        m_imgReady = true;
    } else {
        std::fprintf(stderr, "IMG_Init(PNG) 실패: %s\n", IMG_GetError());
    }

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
    m_textCache.clear();            // 렌더러보다 먼저 텍스트 텍스처 해제.
    m_textures.clear();             // 렌더러보다 먼저 이미지 텍스처 해제(SDL_DestroyTexture).
    if (m_imgReady) IMG_Quit();
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

    // 캐시 키 = 색(8 hex) + 문자열. 같은 (문자열,색)은 텍스처를 재사용하고 dst만 스케일한다.
    char prefix[9];
    std::snprintf(prefix, sizeof(prefix), "%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    std::string key;
    key.reserve(8 + utf8.size());
    key.append(prefix, 8).append(utf8);

    auto it = m_textCache.find(key);
    if (it == m_textCache.end()) {
        const SDL_Color c{color.r, color.g, color.b, color.a};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font.get(), utf8.c_str(), c);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer.get(), surf);
        const int w = surf->w, h = surf->h;
        SDL_FreeSurface(surf);
        if (!tex) return;

        // 무한 증식 방지(동적 상태 텍스트 등) — 임계 초과 시 캐시 비움.
        if (m_textCache.size() > 1024) m_textCache.clear();
        it = m_textCache.emplace(std::move(key),
                                 CachedText{TexturePtr(tex, &SDL_DestroyTexture), w, h}).first;
    }

    const CachedText& e = it->second;
    const float scale = (e.h > 0) ? pixelHeight / static_cast<float>(e.h) : 1.0f;
    const SDL_FRect dst{topLeft.x, topLeft.y,
                        static_cast<float>(e.w) * scale, static_cast<float>(e.h) * scale};
    SDL_RenderCopyF(m_renderer.get(), e.texture.get(), nullptr, &dst);
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

TextureId SDLRenderDevice::LoadTexture(const std::string& path) {
    if (path.empty()) return kInvalidTexture;
    if (const auto it = m_textureCache.find(path); it != m_textureCache.end()) return it->second;

    // SDL은 경로를 UTF-8로 해석하고 윈도우에서 와이드로 변환한다(한글 경로 OK).
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) {
        std::fprintf(stderr, "이미지 로드 실패(%s): %s\n", path.c_str(), IMG_GetError());
        m_textureCache[path] = kInvalidTexture; // 음수 캐시 — 매 프레임 재시도 방지
        return kInvalidTexture;
    }
    SDL_Texture* raw = SDL_CreateTextureFromSurface(m_renderer.get(), surf);
    SDL_FreeSurface(surf);
    if (!raw) {
        std::fprintf(stderr, "텍스처 생성 실패(%s): %s\n", path.c_str(), SDL_GetError());
        m_textureCache[path] = kInvalidTexture;
        return kInvalidTexture;
    }
    const TextureId id = static_cast<TextureId>(m_textures.size());
    m_textures.emplace_back(raw, &SDL_DestroyTexture); // ADR-002: Custom Deleter
    m_textureCache[path] = id;
    return id;
}

void SDLRenderDevice::DrawTexture(TextureId tex, const math::Rect& dst) {
    if (tex < 0 || tex >= static_cast<TextureId>(m_textures.size())) return;
    SDL_Texture* t = m_textures[tex].get();
    if (!t) return;
    const SDL_FRect d{dst.x, dst.y, dst.w, dst.h};
    SDL_RenderCopyF(m_renderer.get(), t, nullptr, &d);
}

math::Vector2D SDLRenderDevice::TextureSize(TextureId tex) const {
    if (tex < 0 || tex >= static_cast<TextureId>(m_textures.size())) return {0.0f, 0.0f};
    SDL_Texture* t = m_textures[tex].get();
    if (!t) return {0.0f, 0.0f};
    int w = 0, h = 0;
    SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
    return {static_cast<float>(w), static_cast<float>(h)};
}

void SDLRenderDevice::Present() {
    SDL_RenderPresent(m_renderer.get());
}

} // namespace gs::platform
