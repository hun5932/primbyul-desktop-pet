# 프림별 (Primbyul)

**[English](README.md) · [한국어](README.ko.md)**

바탕화면을 돌아다니는 사모예드 데스크톱 펫 — Windows · macOS.

[![CI](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)

성견 프림과 어릴 때 프림, 두 가지 모습으로 화면 위를 돌아다니는 네이티브
데스크톱 펫입니다. 직접 그린 512×512 키프레임으로 달리고, 꼬리 흔들고,
장난치고, 지켜보고, 앉고, 자연스럽게 눈을 깜빡입니다. 별도 런타임 없이 작은
단일 실행 파일로 동작하며, 모든 설정은 알림 영역(트레이)과 메뉴 막대에서
조절합니다.

이 저장소는 그 프림별의 수정·빌드·업데이트용 전체 소스입니다.

> GitHub 저장소용 안전 설정이 적용되어 있습니다. 실제 프림 참고사진,
> 인증서, 비밀키, 빌드 결과물은 Git에 올라가지 않습니다.

## 기능

- **두 가지 모습**: 성견 프림 · 어릴 때 프림
- **동작**: 달리기, 꼬리 흔들기, 장난치기, 옆에서 지켜보기, 앉기 + 자연스러운 눈 깜빡임
- **모드**: 자동으로 놀기 · 조용히 지켜보기(달리기 없음) · 얌전히 앉아 있기
- **화면 제어**: 마우스 클릭 통과, 항상 위에 표시, 크기 조절(75 / 100 / 135%), 드래그 이동, 위치 기억, 다중 모니터 복구
- **Windows**: 로그인 시 자동 실행, 탐색기 재시작 후 알림 영역 아이콘 복구, 네이티브 GUI 단일 EXE
- **macOS**: Intel·Apple Silicon Universal 앱, 메뉴 막대 제어, 로그인 자동 실행
- **가벼움**: 현재 동작의 4프레임만 메모리에 로드

## 스크린샷

> 여기에 프림별이 도는 GIF나 정지 이미지 한 장을 넣어 주세요.
> (예: 실제 동작 GIF, 또는 `assets/frames/adult/wag/0.png` 같은 프레임)

## 받기 · 실행

현재는 소스에서 직접 빌드하는 것이 기본 경로입니다(아래 "가장 빠른 시작").
서명·공증된 릴리스 빌드는 버전 태그를 붙이면 GitHub Releases에 초안으로
올라옵니다. 자세한 절차는 [VERSION-AND-RELEASE.md](docs/VERSION-AND-RELEASE.md)를
참고하세요.

## 설계 원칙

- 플랫폼 코드와 이미지 자산을 분리합니다.
- 빌드 결과는 `dist/`에만 생성합니다.
- 버전은 `config/project.json`을 기준으로 한 번에 변경합니다.
- Windows와 macOS를 같은 512×512 프레임으로 관리합니다.
- 배포 전에 소스·프레임·실행 형식·ZIP 권한을 자동 검사합니다.

## GitHub에 올리기

처음에는 **비공개 저장소**로 올리는 것을 권장합니다. Windows에서 Git과
GitHub CLI를 설치한 뒤:

```powershell
.\tools\publish_github.ps1
```

Mac/Linux:

```bash
./tools/publish_github.sh
```

스크립트는 `primbyul-desktop-pet`이라는 비공개 저장소를 만들고 첫 커밋을
업로드합니다. 올리기 전에 참고사진·비밀키 유출 검사(`validate_github.py`,
`validate_source.py`)를 자동으로 실행하며, 검사에 실패하면 업로드를
중단합니다. 공개 저장소나 저장소 이름 변경, GitHub Desktop/Web 업로드,
자동 릴리스 방법은 [GITHUB.md](docs/GITHUB.md)를 확인하세요.

## 가장 빠른 시작

필요한 프로그램:

1. Python 3.11 이상
2. Zig 컴파일러

압축을 `C:\workspace\Primbyul` 같은 폴더에 푼 뒤 터미널에서:

```powershell
cd C:\workspace\Primbyul
python tools\build_all.py
```

Zig가 PATH에 없다면 현재 터미널에서 실행파일 위치를 지정할 수 있습니다.

```powershell
$env:ZIG = "C:\tools\zig\zig.exe"
python tools\build_all.py
```

생성 결과:

```text
dist/
├─ Primbyul_v1.5.exe
├─ Primbyul_v1.5_macOS/
└─ Primbyul_v1.5_macOS.zip
```

Windows만 빌드:

```powershell
python tools\build_windows.py
```

macOS만 빌드:

```powershell
python tools\build_macos.py
```

## 소스 백업 ZIP 만들기

코드·문서·이미지 원본만 묶고 `build/`, `dist/`, 캐시와 EXE는 제외합니다.

```powershell
python tools\package_source.py
```

생성된 소스 ZIP에는 각 파일의 SHA-256을 기록한
`SOURCE-CHECKSUMS.sha256`이 들어갑니다. ZIP을 다시 푼 뒤 다음 명령으로
누락이나 손상을 확인할 수 있습니다.

```powershell
python tools\verify_checksums.py
python tools\validate_source.py
```

## 다음 버전 만들기

```powershell
python tools\bump_version.py 1.6.0
python tools\build_all.py
```

`bump_version.py`는 Windows 리소스·manifest, macOS Info.plist·메뉴 표시와
중앙 설정을 함께 변경합니다. 변경 후 `CHANGELOG.md`에는 사람이 직접 변경
내용을 기록합니다.

## 이미지 동작 수정

1. `assets/source-keyframes/<appearance>/<action>/key-0.png`부터
   `key-3.png`까지 교체합니다.
2. 이미지 도구 의존성을 설치합니다.

```powershell
python -m pip install -r requirements-assets.txt
python tools\build_assets.py
python tools\build_all.py
```

상세 규칙은 [ANIMATION-WORKFLOW.md](docs/ANIMATION-WORKFLOW.md)를 먼저 읽으세요.

## 문서 지도

- [BUILD.md](docs/BUILD.md): 설치, 빌드, 생성물
- [PROJECT-STRUCTURE.md](docs/PROJECT-STRUCTURE.md): 폴더별 역할
- [CODE-WALKTHROUGH.md](docs/CODE-WALKTHROUGH.md): Windows/macOS 코드 설명
- [UPDATE-GUIDE.md](docs/UPDATE-GUIDE.md): 수정 유형별 절차
- [ANIMATION-WORKFLOW.md](docs/ANIMATION-WORKFLOW.md): 프레임 제작·교체 규칙
- [VERSION-AND-RELEASE.md](docs/VERSION-AND-RELEASE.md): 버전과 릴리스 관리
- [GITHUB.md](docs/GITHUB.md): 저장소 생성, 자동 빌드, 태그 릴리스
- [SIGNING.md](docs/SIGNING.md): Windows 서명·Mac 서명/공증
- [KNOWN-LIMITATIONS.md](docs/KNOWN-LIMITATIONS.md): 현재 한계와 주의점
- [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md): 빌드·실행 문제 해결

## 중요한 사실

- 현재 소스는 Windows x64와 macOS Universal(x86_64 + arm64)을 만듭니다.
- Windows EXE는 별도 PowerShell/BAT를 실행하지 않는 네이티브 GUI입니다.
- Mac 앱은 메뉴 막대에서 모든 설정을 복구할 수 있습니다.
- 빌드 스크립트는 코드서명 인증서를 포함하지 않습니다.
- 원본 참고사진은 Git에서 제외되며 실행·빌드에는 필요하지 않습니다.
- 현재 공개 재사용 라이선스는 지정하지 않았습니다. 공개 오픈소스로
  배포하려면 `docs/GITHUB.md`의 라이선스 항목을 먼저 결정하세요.
- 실제 배포 전에는 각 운영체제에서 `docs/VERSION-AND-RELEASE.md`의 수동
  스모크 테스트를 반드시 실행해야 합니다.
