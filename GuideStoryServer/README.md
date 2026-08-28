# GuideStoryServer — 계정 / 채팅 서버

클라이언트(`GuideStoryGame.exe`)와 **같은 프로토콜 헤더**(`GuideStory/src/net/Protocol.h`)를
컴파일하는 콘솔 서버다. ADR-001(데디케이트 서버, 서버 권위)의 1단계 — 인증된 세션과
그들 사이의 메시지까지를 맡는다. 월드 상태(누가 어느 맵에 있는가)는 아직 서버가 모른다.

## 지금 하는 일

| 기능 | 내용 |
|------|------|
| 가입 | 아이디/비밀번호/닉네임. 아이디·닉네임 모두 대소문자 무시 유일 |
| 로그인 | PBKDF2-HMAC-SHA256(10만 회) + 솔트. 중복 접속 거부 |
| 채팅 | 전체 / 귓속말(닉네임 지정) / 시스템(서버만 생성) |
| 로그 | `chat_log` 테이블에 비동기 기록 |
| 하트비트 | 받은 그대로 돌려준다(연결 유지 + 생존 확인) |

## 빌드

솔루션 전체를 빌드하면 함께 만들어진다.

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" GuideStory.sln /p:Configuration=Debug /p:Platform=x64
```

서버만 따로 빌드할 수도 있다. **SDL/vcpkg 복원이 필요 없다** — 서버는 그래픽을 쓰지 않고
SQLite 는 `ThirdParty/sqlite` 의 앰알가메이션을 그대로 컴파일한다.

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" GuideStoryServer\GuideStoryServer.vcxproj /p:Configuration=Debug /p:Platform=x64
```

## 실행

```powershell
.\x64\Debug\GuideStoryServer.exe            # 기본 7777 포트, guidestory.db
.\x64\Debug\GuideStoryServer.exe 7777 C:\temp\guidestory.db
```

클라이언트가 붙을 주소는 `assets/config/server.txt` 에서 바꾼다(빌드 불필요).

```
GSSERVER 1
HOST 127.0.0.1
PORT 7777
```

Ctrl+C 로 내리면 큐에 남은 채팅 로그를 마저 쓰고 종료한다.

## 파일

| 파일 | 역할 |
|------|------|
| `src/Server.cpp` | accept 루프 + 접속당 스레드 하나 + 패킷 핸들러 |
| `src/Session.h/.cpp` | 접속 하나의 상태와 세션 목록(모든 send 의 직렬화 지점) |
| `src/Accounts.h/.cpp` | 계정 저장소(동기 SQLite, synchronous=FULL) |
| `src/ChatLog.h/.cpp` | 채팅 로그(비동기 큐 + 라이터 스레드 1개) |
| `src/Crypto.h/.cpp` | SHA-256 / HMAC / PBKDF2 (외부 라이브러리 없음) |

프로토콜과 프레이밍은 `GuideStory/src/net/` 에 있다 — 클라이언트와 공유하는 소스다.

## DB 스키마

같은 파일 하나에 두 테이블이 들어가지만 **커넥션과 보증 수준은 다르다.**

- `accounts` — 동기 쓰기, `synchronous=FULL`. "가입했는데 계정이 없다" 는 있을 수 없다.
- `chat_log` — 비동기 큐, `synchronous=NORMAL`. 비정상 종료 시 마지막 몇 줄은 잃어도 된다.

이 비대칭이 두 모듈을 따로 둔 이유다. 자세한 근거는 각 헤더 주석에 있다.

## 측정값

| 항목 | 값 | 메모 |
|------|----|------|
| PBKDF2 1회 (10만 반복) | Release **88ms** / Debug **404ms** | 로그인·가입 1건당 서버 CPU. Debug에서 로그인이 굼뜬 이유 |

한 코어가 처리할 수 있는 로그인이 Release 기준 초당 10여 건이라는 뜻이다.
로그인은 접속당 한 번뿐이라 지금 규모에는 충분하지만, 로그인 폭주는 그 자체로 CPU 고갈이 된다.

## 알려진 한계

- **비밀번호가 평문으로 전송된다.** 저장은 해시하지만 전송 구간에 암호화가 없다.
  실제로 쓰는 비밀번호를 넣지 말 것 — TLS 도입은 `tech-debt-tracker.md` D-008.
- **모든 send 가 세션 목록 락 안에서 일어난다.** 느린 수신자 하나가 접속/종료를 막는다.
  접속자가 늘면 세션별 송신 큐로 바꿔야 한다 — D-009.
- **접속 1건당 스레드 1개.** 구현이 단순한 대신 접속자 수에 비례해 스레드가 는다.
  IOCP/epoll 과의 비교가 ADR-001 의 증명 과제이고, 이 코드가 그 기준선이다.

## 출처

`Unreal-MOU/MOU_Server` 를 GuideStory 자체 엔진에 맞게 옮긴 것이다.
언리얼 의존(FSocket/TQueue/UObject)은 없앴고, 4인 리슨서버 co-op 전제의 기능
(방/로비, 팀·사망 채널, 친구·메신저)은 지속 월드에 맞지 않아 가져오지 않았다.
