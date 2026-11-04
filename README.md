# DawnRift

리그 오브 레전드를 소재로 만든 5:5 MOBA 프로젝트입니다. 자체 DirectX 11 C++ 클라이언트, C# 권위 서버, WPF 데스크톱 런처로 구성했습니다. 개인 학습 프로젝트로 판매하거나 배포하지 않습니다.

이동과 전투는 서버가 확정하고, 클라이언트는 입력과 렌더링을 맡습니다. 런처에서 방을 만들고 챔피언을 고른 뒤, 서버가 발급한 입장 토큰으로 게임 클라이언트를 실행합니다.

## WPF 런처

`Client/Launcher`는 .NET 8 WPF로 만든 독립 데스크톱 앱이며, 외부 프레임워크 없이 MVVM으로 구성했습니다.

- `Views`는 XAML 바인딩과 템플릿으로 화면을 그리고, 코드 비하인드에 화면 로직을 두지 않습니다.
- `ViewModels`가 화면 상태와 커맨드를 담당하고, `Mvvm`에 속성 알림과 커맨드 기반 클래스를 직접 구현했습니다.
- `Services`에서 소켓, 게임 프로세스, 창 제어를 인터페이스로 분리했습니다.
- 비동기 TCP와 Protobuf로 서버와 통신하고, 수신 패킷을 `Dispatcher`로 UI 스레드에 전달합니다.
- 게임 클라이언트를 실행하고, 종료를 감지해 로비로 복귀합니다.

## 구성

| 폴더 | 내용 |
|---|---|
| `Client/Launcher` | WPF 런처 |
| `Client/EngineCore` | 렌더링, 컴포넌트, 애니메이션, 입력 등 엔진 코드 |
| `Client/GameCoding2` | 챔피언, 스킬, HUD, VFX, 패킷 처리 |
| `Client/ServerCore` | IOCP 기반 C++ 네트워크 코드 |
| `Client/Shaders`, `Client/Resources/Particles` | HLSL 셰이더 |
| `Server/Server` | 로비, 게임 방, 전투 판정, 봇, 재접속 |
| `Server/ServerCore`, `Server/PacketGenerator` | C# 네트워크 기반과 패킷 처리 코드 생성기 |
| `Server/Common` | Protobuf 메시지 정의 |

## 참고

- 소스 코드만 공개합니다. 리소스, 데이터 변환 도구, 테스트, 생성된 패킷 코드, 외부 라이브러리는 포함하지 않아 이 저장소만으로는 빌드되지 않습니다.
