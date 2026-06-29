#include "platform/SDLRenderDevice.h"

#include "platform/SDLWindow.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

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

namespace {
// 애니메이션 한 프레임의 기본 지속(ms). GIF가 0/미지정 지연을 줄 때의 폴백.
constexpr int kDefaultFrameMs = 100;

// 확장자가 .gif인지(대소문자 무시). 애니메이션 디코드를 시도할지 판단용.
bool HasGifExt(const std::string& path) {
    if (path.size() < 4) return false;
    const std::string ext = path.substr(path.size() - 4);
    return (ext == ".gif" || ext == ".GIF" || ext == ".Gif");
}

// UTF-8 문자열 ↔ filesystem::path 변환(윈도우에서 한글 경로 보존; C++20 u8string 경유, deprecated u8path 회피).
std::filesystem::path Utf8ToPath(const std::string& s) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}
std::string PathToUtf8(const std::filesystem::path& p) {
    const std::u8string u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
}

// 소문자 확장자(".png" 등). 비교용.
std::string LowerExt(const std::filesystem::path& p) {
    std::string e = PathToUtf8(p.extension());
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e;
}
} // namespace

TextureId SDLRenderDevice::LoadTexture(const std::string& path) {
    if (path.empty()) return kInvalidTexture;
    if (const auto it = m_textureCache.find(path); it != m_textureCache.end()) return it->second;

    // SDL은 경로를 UTF-8로 해석하고 윈도우에서 와이드로 변환한다(한글 경로 OK).
    LoadedImage img;

    // 디렉터리 = PNG 프레임 시퀀스 애니메이션. 파일명이 "<시작ms>_<끝ms>.png"면 그 차이가 프레임 지속,
    // 시작값으로 정렬한다(투명 PNG 시퀀스 = GIF 1비트 투명의 대안, 부드러운 알파). 규칙에 안 맞는 이름은
    // 알파벳 순서 + 기본 지속으로 폴백.
    std::error_code dec;
    if (std::filesystem::is_directory(Utf8ToPath(path), dec)) {
        struct FrameFile { long start; int durMs; std::string file; };
        std::vector<FrameFile> ff;
        for (const auto& e : std::filesystem::directory_iterator(Utf8ToPath(path), dec)) {
            if (!e.is_regular_file() || LowerExt(e.path()) != ".png") continue;
            const std::string stem = PathToUtf8(e.path().stem());
            // "<시작>_<끝>" 파싱: '_' 앞뒤 정수. 규칙에 안 맞으면 폴백.
            long a = 0, b = 0;
            bool parsed = false;
            if (const auto us = stem.find('_'); us != std::string::npos && us > 0) {
                char* e1 = nullptr;
                char* e2 = nullptr;
                a = std::strtol(stem.c_str(), &e1, 10);
                b = std::strtol(stem.c_str() + us + 1, &e2, 10);
                parsed = (e1 == stem.c_str() + us) && (e2 != stem.c_str() + us + 1);
            }
            FrameFile f;
            f.start = parsed ? a : static_cast<long>(ff.size());
            f.durMs = (parsed && b > a) ? static_cast<int>(b - a) : kDefaultFrameMs;
            f.file  = PathToUtf8(e.path());
            ff.push_back(std::move(f));
        }
        std::sort(ff.begin(), ff.end(), [](const FrameFile& x, const FrameFile& y) {
            return (x.start != y.start) ? x.start < y.start : x.file < y.file;
        });
        // 로딩 최적화: PNG 디코드(IMG_Load)는 CPU 작업이라 여러 스레드로 병렬 처리한다 —
        // 프레임 수가 많은 배경/몹 폴더의 첫 렌더 히치를 코어 수만큼 줄인다. SDL_Texture 생성은
        // 렌더러(GPU 컨텍스트)에 묶여 메인 스레드 전용이므로 업로드만 순차로 한다.
        std::vector<SDL_Surface*> surfaces(ff.size(), nullptr);
        {
            const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
            const std::size_t nthreads = std::min<std::size_t>(hw, ff.size());
            std::atomic<std::size_t> next{0};
            auto worker = [&] {
                for (std::size_t i = next.fetch_add(1); i < ff.size(); i = next.fetch_add(1))
                    surfaces[i] = IMG_Load(ff[i].file.c_str());
            };
            std::vector<std::thread> pool;
            for (std::size_t t = 1; t < nthreads; ++t) pool.emplace_back(worker);
            worker();                       // 호출 스레드도 한 몫 거든다
            for (auto& th : pool) th.join();
        }
        for (std::size_t i = 0; i < ff.size(); ++i) {
            SDL_Surface* fs = surfaces[i];
            if (!fs) continue;
            SDL_Texture* ft = SDL_CreateTextureFromSurface(m_renderer.get(), fs);
            const int fw = fs->w, fh = fs->h;
            SDL_FreeSurface(fs);
            if (!ft) continue;
            SDL_SetTextureBlendMode(ft, SDL_BLENDMODE_BLEND); // PNG 알파를 배경과 합성
            img.frames.emplace_back(ft, &SDL_DestroyTexture);
            img.delaysMs.push_back(ff[i].durMs);
            img.totalMs += ff[i].durMs;
            if (img.w == 0) { img.w = fw; img.h = fh; }
        }
        if (img.frames.empty())
            std::fprintf(stderr, "프레임 폴더에 PNG 없음(%s)\n", path.c_str());
    }

    // 애니메이션 GIF: 모든 프레임을 디코드해 프레임별 텍스처 + 지연을 보관(DrawTexture가 시간으로 선택).
    if (img.frames.empty() && HasGifExt(path)) {
        if (IMG_Animation* anim = IMG_LoadAnimation(path.c_str())) {
            for (int i = 0; i < anim->count; ++i) {
                SDL_Surface* fs = anim->frames[i];
                if (!fs) continue;
                SDL_Texture* ft = SDL_CreateTextureFromSurface(m_renderer.get(), fs);
                if (!ft) continue;
                SDL_SetTextureBlendMode(ft, SDL_BLENDMODE_BLEND); // GIF 투명 픽셀(알파)을 살려 배경과 합성
                const int d = (anim->delays && anim->delays[i] > 0) ? anim->delays[i] : kDefaultFrameMs;
                img.frames.emplace_back(ft, &SDL_DestroyTexture); // ADR-002: Custom Deleter
                img.delaysMs.push_back(d);
                img.totalMs += d;
                if (img.w == 0) { img.w = fs->w; img.h = fs->h; }
            }
            IMG_FreeAnimation(anim); // 서피스 소유는 anim → 텍스처로 옮겨 담았으니 해제.
        }
    }

    // 정지 이미지(PNG 등) 또는 애니메이션 디코드 실패 폴백.
    if (img.frames.empty()) {
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (!surf) {
            std::fprintf(stderr, "이미지 로드 실패(%s): %s\n", path.c_str(), IMG_GetError());
            m_textureCache[path] = kInvalidTexture; // 음수 캐시 — 매 프레임 재시도 방지
            return kInvalidTexture;
        }
        SDL_Texture* raw = SDL_CreateTextureFromSurface(m_renderer.get(), surf);
        const int w = surf->w, h = surf->h;
        SDL_FreeSurface(surf);
        if (!raw) {
            std::fprintf(stderr, "텍스처 생성 실패(%s): %s\n", path.c_str(), SDL_GetError());
            m_textureCache[path] = kInvalidTexture;
            return kInvalidTexture;
        }
        SDL_SetTextureBlendMode(raw, SDL_BLENDMODE_BLEND); // 알파(투명 PNG 등)를 배경과 합성
        img.frames.emplace_back(raw, &SDL_DestroyTexture);
        img.delaysMs.push_back(0);
        img.totalMs = 0; // 정지
        img.w = w; img.h = h;
    }

    const TextureId id = static_cast<TextureId>(m_textures.size());
    m_textures.push_back(std::move(img));
    m_textureCache[path] = id;
    return id;
}

void SDLRenderDevice::DrawTexture(TextureId tex, const math::Rect& dst) {
    if (tex < 0 || tex >= static_cast<TextureId>(m_textures.size())) return;
    const LoadedImage& img = m_textures[tex];
    if (img.frames.empty()) return;

    // 현재 프레임 선택: 정지(프레임 1개/totalMs=0)면 0번, 애니메이션이면 벽시계 시간으로 순환.
    std::size_t idx = 0;
    if (img.frames.size() > 1 && img.totalMs > 0) {
        int t = static_cast<int>(SDL_GetTicks() % static_cast<Uint32>(img.totalMs));
        for (std::size_t i = 0; i < img.delaysMs.size(); ++i) {
            t -= img.delaysMs[i];
            if (t < 0) { idx = i; break; }
        }
    }
    SDL_Texture* t = img.frames[idx].get();
    if (!t) return;
    const SDL_FRect d{dst.x, dst.y, dst.w, dst.h};
    SDL_RenderCopyF(m_renderer.get(), t, nullptr, &d);
}

math::Vector2D SDLRenderDevice::TextureSize(TextureId tex) const {
    if (tex < 0 || tex >= static_cast<TextureId>(m_textures.size())) return {0.0f, 0.0f};
    const LoadedImage& img = m_textures[tex];
    return {static_cast<float>(img.w), static_cast<float>(img.h)};
}

void SDLRenderDevice::Present() {
    SDL_RenderPresent(m_renderer.get());
}

} // namespace gs::platform
