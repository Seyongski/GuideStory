# SQLite 앰알가메이션 (외부 코드 — 수정하지 말 것)

`sqlite3.c` / `sqlite3.h` 는 **SQLite 3.47.1** 원본이다. `GuideStoryServer` 의 계정/채팅 로그 저장소가 쓴다.

## 왜 vcpkg 가 아니라 소스를 동봉하는가

클라이언트(`GuideStoryGame`/`GuideStoryEditor`)는 SDL2 를 vcpkg 매니페스트로 받지만,
서버는 **vcpkg 없이도 단독으로 빌드되어야 한다**:

- 서버는 SDL 을 쓰지 않는다. vcpkg 를 걸면 그래픽 의존성 트리(SDL2/freetype/libpng…)를
  복원해야 서버가 빌드되는데, 서버만 배포하는 상황(데디케이트 서버, ADR-001)에서 낭비다.
- 앰알가메이션은 `.c` 파일 하나라 어느 툴체인에서도 그대로 컴파일된다.
  나중에 리눅스 데디케이트로 옮길 때 `g++ *.cpp sqlite3.c` 로 끝난다.

트레이드오프: 저장소에 9MB 소스가 들어온다. SQLite 는 릴리스 주기가 길고 로컬 수정이 없어서
갱신 비용이 사실상 0 이라 감수한다.

## 출처

`Unreal-MOU/MOU_Server/ThirdParty/sqlite` 에서 그대로 가져왔다.
그쪽은 언리얼 엔진 5.8 이 동봉한 사본(`SQLiteCore/Private/sqlite/sqlite3.inl`)을 복사한 것이고,
내용은 sqlite.org 가 배포하는 앰알가메이션과 동일하다.

## 라이선스

퍼블릭 도메인이다. 저작권 표시나 라이선스 파일 동봉 의무가 없다.
<https://www.sqlite.org/copyright.html>

## 갱신 방법

sqlite.org 의 amalgamation zip 을 받아 두 파일만 덮어쓴다. 로컬 수정은 없다.
