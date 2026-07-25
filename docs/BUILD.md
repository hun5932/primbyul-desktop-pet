# 빌드 가이드

## 1. 필수 도구

일반 빌드:

- Python 3.11 이상
- Zig 컴파일러

이미지 원본을 다시 정렬할 때만 추가:

- NumPy
- Pillow

```powershell
python -m pip install -r requirements-assets.txt
```

현재 배포판은 Zig `0.14.0-dev.2456+a68119f8f`로 엄격 경고
(`-Wall -Wextra -Werror`)를 적용해 검증했습니다. 다른 Zig 버전에서
링커 옵션이 바뀌면 동일 계열 버전으로 먼저 재현하세요.

## 2. Zig 경로

Zig가 PATH에 있으면 별도 설정이 필요 없습니다.

```powershell
zig version
```

PATH를 바꾸지 않으려면:

```powershell
$env:ZIG = "C:\tools\zig\zig.exe"
```

macOS/Linux:

```bash
export ZIG=/opt/zig/zig
```

## 3. 전체 빌드

```powershell
python tools\build_all.py
```

실행 순서:

1. `validate_source.py`
2. `build_windows.py`
3. `build_macos.py`
4. `validate_release.py`

중간 단계가 실패하면 뒤 단계는 실행되지 않습니다.

## 4. Windows 빌드

```powershell
python tools\build_windows.py
```

내부 처리:

1. `primbyul.rc`를 UTF-8 코드페이지로 컴파일
2. Win32 C 코드와 리소스를 x86_64 Windows GUI PE로 링크
3. `dist/Primbyul_v1.5.exe` 생성

필요한 Windows DLL은 운영체제 기본 구성요소입니다.

- USER32
- GDI32
- GDI+
- SHELL32
- OLE32
- ADVAPI32

별도 런타임 설치나 PowerShell 창은 필요하지 않습니다.

## 5. macOS 빌드

```powershell
python tools\build_macos.py
```

Mac이 아닌 PC에서도 Zig 크로스 컴파일이 가능합니다.

1. x86_64 Mach-O 생성
2. arm64 Mach-O 생성
3. 두 파일을 Universal/FAT 바이너리로 결합
4. `.app` 번들과 ICNS 아이콘 구성
5. 실행 권한을 보존한 ZIP 생성

결과:

```text
dist/Primbyul_v1.5_macOS.zip
```

## 6. 캐시와 깨끗한 빌드

각 플랫폼 스크립트는 프로젝트 내부의 해당 빌드 폴더만 초기화합니다.

```text
build/windows/
build/macos/
```

소스, 자산, 다른 폴더를 재귀 삭제하지 않도록 안전 범위를 검사합니다.

## 7. 정상 빌드의 기준

- Windows: PE32+ x86-64, GUI subsystem
- macOS: x86_64 + arm64 두 슬라이스
- macOS 앱: 56개 프레임과 실행 권한
- ZIP: CRC 오류 없음
- 버전: 중앙 설정, RC, manifest, plist, 메뉴가 일치
- 프레임: 512×512, 테두리 알파 잘림 없음

## 8. 소스 패키지 만들기와 확인

```powershell
python tools\package_source.py
```

`dist/Primbyul_v1.5_Source.zip`이 생성됩니다. 이 ZIP에는 완성 바이너리와
컴파일 캐시가 들어가지 않으며, 대신 모든 포함 파일의 SHA-256 목록이
`SOURCE-CHECKSUMS.sha256`로 추가됩니다.

새 폴더에 ZIP을 푼 뒤:

```powershell
python tools\verify_checksums.py
python tools\validate_source.py
```

두 검사가 모두 통과해야 백업·전달용 소스가 완전한 것으로 봅니다.
