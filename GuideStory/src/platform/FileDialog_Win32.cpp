// FileDialog의 Win32 구현 (ADR-006: 네이티브 의존은 platform/ 뒤에만).
// 네이티브 공통 대화상자(GetOpenFileName/GetSaveFileName)와 실행 파일 기준 경로 해석.
#include "platform/FileDialog.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

#include <filesystem>

#pragma comment(lib, "Comdlg32.lib") // 공통 대화상자 — 정적 lib를 통해 exe 링크에 전파됨

namespace gs::platform {

namespace {

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                        nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                          s.data(), n, nullptr, nullptr);
    return s;
}

std::filesystem::path ExeDir() {
    wchar_t buf[MAX_PATH] = {0};
    const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(std::wstring(buf, n)).parent_path();
}

// 실행 파일에서 위로 올라가며 GuideStory.sln이 있는 폴더(=리포 루트)를 찾는다.
// 프로세스 수명 동안 불변 → 1회만 계산하고 캐시한다(매 프레임 경로 해석에서 파일시스템 탐색 회피).
const std::filesystem::path& RepoRoot() {
    static const std::filesystem::path root = [] {
        std::error_code ec;
        for (std::filesystem::path d = ExeDir(); !d.empty(); d = d.parent_path()) {
            if (std::filesystem::exists(d / "GuideStory.sln", ec)) return d;
            if (d == d.root_path()) break;
        }
        return ExeDir(); // 폴백: 배포본 등 .sln이 없을 때
    }();
    return root;
}

// <repo>/assets/<sub> 경로(생성하지 않음 — 읽기 해석용).
std::filesystem::path AssetsPathW(const std::string& sub) {
    std::filesystem::path dir = RepoRoot() / L"assets";
    if (!sub.empty()) dir /= Utf8ToWide(sub);
    return dir;
}

// 공통 대화상자 필터 문자열: "라벨\0패턴\0\0".
std::wstring BuildFilter(const std::wstring& label, const std::wstring& pattern) {
    std::wstring f = label;
    f.push_back(L'\0');
    f += pattern;
    f.push_back(L'\0');
    f.push_back(L'\0');
    return f;
}

} // namespace

std::string AssetsDir(const std::string& sub) {
    const std::filesystem::path dir = AssetsPathW(sub);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec); // 대화상자 초기 폴더·저장 대상 보장
    return WideToUtf8(dir.wstring());
}

namespace {
// name을 자산 하위 폴더(sub)에 해석. 절대 경로면 그대로.
// 읽기 경로 해석이라 디렉터리를 생성하지 않는다(매 프레임 호출되는 핫패스).
std::string ResolveAsset(const std::string& sub, const std::string& name) {
    if (name.empty()) return name;
    const std::filesystem::path p(Utf8ToWide(name));
    if (p.is_absolute()) return name;
    return WideToUtf8((AssetsPathW(sub) / Utf8ToWide(name)).wstring());
}
} // namespace

std::string MapPath(const std::string& name)        { return ResolveAsset("maps", name); }
std::string BackgroundPath(const std::string& name) { return ResolveAsset("backgrounds", name); }
std::string MobPath(const std::string& name)        { return ResolveAsset("mob", name); }

std::optional<std::string> OpenFileDialog(const std::string& title,
                                          const std::string& filterLabel,
                                          const std::string& filterPattern,
                                          const std::string& initialDir) {
    wchar_t file[2048] = {0};
    const std::wstring wtitle = Utf8ToWide(title);
    const std::wstring filter = BuildFilter(Utf8ToWide(filterLabel), Utf8ToWide(filterPattern));
    const std::wstring wdir = Utf8ToWide(initialDir);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ::GetActiveWindow();
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(sizeof(file) / sizeof(file[0]));
    ofn.lpstrTitle = wtitle.empty() ? nullptr : wtitle.c_str();
    ofn.lpstrInitialDir = wdir.empty() ? nullptr : wdir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (::GetOpenFileNameW(&ofn)) return WideToUtf8(file);
    return std::nullopt;
}

std::optional<std::string> SaveFileDialog(const std::string& title,
                                          const std::string& filterLabel,
                                          const std::string& filterPattern,
                                          const std::string& defaultExt,
                                          const std::string& initialDir) {
    wchar_t file[2048] = {0};
    const std::wstring wtitle = Utf8ToWide(title);
    const std::wstring filter = BuildFilter(Utf8ToWide(filterLabel), Utf8ToWide(filterPattern));
    const std::wstring wdir = Utf8ToWide(initialDir);
    const std::wstring wext = Utf8ToWide(defaultExt);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ::GetActiveWindow();
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(sizeof(file) / sizeof(file[0]));
    ofn.lpstrTitle = wtitle.empty() ? nullptr : wtitle.c_str();
    ofn.lpstrInitialDir = wdir.empty() ? nullptr : wdir.c_str();
    ofn.lpstrDefExt = wext.empty() ? nullptr : wext.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (::GetSaveFileNameW(&ofn)) return WideToUtf8(file);
    return std::nullopt;
}

SavePrompt AskSaveChanges(const std::string& title, const std::string& message) {
    const std::wstring wtitle = Utf8ToWide(title);
    const std::wstring wmsg = Utf8ToWide(message);
    const int r = ::MessageBoxW(::GetActiveWindow(), wmsg.c_str(), wtitle.c_str(),
                                MB_YESNOCANCEL | MB_ICONWARNING | MB_TASKMODAL);
    switch (r) {
        case IDYES: return SavePrompt::Save;
        case IDNO:  return SavePrompt::Discard;
        default:    return SavePrompt::Cancel; // IDCANCEL · ESC · 닫기
    }
}

} // namespace gs::platform
