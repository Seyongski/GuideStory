#pragma once

#include <optional>
#include <string>

// 네이티브 파일 대화상자 + 프로젝트 자산 경로 해석.
// ADR-006: 플랫폼(Win32) 의존은 platform/ 뒤에만 둔다 — 이 헤더는 std::string/optional만 노출하고,
// 구현(FileDialog_Win32.cpp)에서만 <windows.h>/<commdlg.h>를 사용한다.
namespace gs::platform {

// 윈도우 "열기" 대화상자. 사용자가 취소하면 std::nullopt, 선택하면 절대 경로(UTF-8).
//  - filterPattern 예: "*.gsmap"
//  - initialDir 비어 있으면 시스템 기본 위치.
std::optional<std::string> OpenFileDialog(const std::string& title,
                                          const std::string& filterLabel,
                                          const std::string& filterPattern,
                                          const std::string& initialDir);

// 윈도우 "다른 이름으로 저장" 대화상자. 취소 시 std::nullopt, 선택 시 절대 경로(UTF-8).
//  - defaultExt 예: "gsmap" (사용자가 확장자를 안 적으면 자동 부착)
std::optional<std::string> SaveFileDialog(const std::string& title,
                                          const std::string& filterLabel,
                                          const std::string& filterPattern,
                                          const std::string& defaultExt,
                                          const std::string& initialDir);

// 프로젝트 자산 루트(<repo>/assets) 하위 폴더의 절대 경로. 폴더가 없으면 생성한다.
// 루트는 실행 파일에서 위로 올라가며 GuideStory.sln을 찾아 결정한다(빌드 구성 무관).
// 못 찾으면 실행 파일 폴더의 assets로 폴백한다.
//  예: AssetsDir("maps") → "<repo>/assets/maps"
std::string AssetsDir(const std::string& sub = std::string());

// 맵 파일명을 자산 맵 폴더(AssetsDir("maps"))에 해석한다.
// name이 이미 절대 경로면 그대로 반환(포탈 대상·게임 기본 맵 등 맨 파일명 해석용).
std::string MapPath(const std::string& name);

// 배경 이미지 파일명을 자산 배경 폴더(AssetsDir("backgrounds"))에 해석한다.
// name이 절대 경로면 그대로 반환. (맵에는 파일명만 저장 → 게임이 이 함수로 해석)
std::string BackgroundPath(const std::string& name);

} // namespace gs::platform
