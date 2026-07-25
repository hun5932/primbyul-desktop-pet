# 프로젝트 구조

```text
Primbyul_v1.5_Source/
├─ config/
│  └─ project.json
├─ src/
│  ├─ windows/
│  │  ├─ primbyul.c
│  │  ├─ primbyul.rc
│  │  └─ Primbyul.exe.manifest
│  └─ macos/
│     ├─ primbyul_mac.c
│     └─ Info.plist
├─ assets/
│  ├─ frames/
│  ├─ source-keyframes/
│  ├─ icons/
│  └─ reference-photos/
├─ tools/
├─ docs/
├─ build/
└─ dist/
```

## `config/project.json`

버전, 제품명, 대상 플랫폼, macOS 최소 버전, 번들 ID의 중앙 설정입니다.
버전은 직접 여러 파일을 고치지 말고 `bump_version.py`로 변경합니다.

## `src/windows`

- `primbyul.c`: Win32 창, 렌더링, 상태 머신, 설정, 자동 실행, 트레이 메뉴
- `primbyul.rc`: PNG·ICO·manifest·버전 정보의 EXE 내장 목록
- `Primbyul.exe.manifest`: 권한, Windows 호환성, Per-Monitor DPI

Windows는 이미지 파일을 EXE 리소스로 내장하므로 배포 시 EXE 한 개만
있어도 실행됩니다.

## `src/macos`

- `primbyul_mac.c`: AppKit 창·메뉴 막대·상태 머신·설정·로그인 자동 실행
- `Info.plist`: 앱 이름, 번들 ID, 버전, 최소 OS, Dock 숨김

macOS는 `.app` 번들 내부 `Resources/frames`에서 PNG를 읽습니다.

## `assets/frames`

빌드에 직접 사용되는 512×512 투명 PNG입니다.

- 성견: 7개 폴더 × 4장
- 퍼피: 7개 폴더 × 4장
- `run-left`는 macOS용 미러 프레임

Windows는 오른쪽 달리기를 실행 중 좌우 반전하므로 `run-left`를 EXE에
중복 내장하지 않습니다.

## `assets/source-keyframes`

프레임 재정렬 도구의 입력입니다. 각 모습·동작마다 `key-0.png`부터
`key-3.png`까지 네 장이 있어야 합니다.

## `assets/reference-photos`

AI 또는 수작업으로 프레임을 다시 제작할 때 정체성을 확인하는 사진입니다.
실행파일 빌드에는 포함되지 않습니다.

## `tools`

- `build_all.py`: 전체 검증·빌드
- `build_windows.py`: Windows EXE
- `build_macos.py`: Universal Mac 앱/ZIP
- `build_assets.py`: 키프레임 정렬·누끼 프레임·아이콘 재생성
- `bump_version.py`: 버전 일괄 변경
- `validate_source.py`: 빌드 전 구조 검사
- `validate_release.py`: 빌드 후 바이너리·ZIP 검사
- `validate_github.py`: Git 추적 파일·개인정보·워크플로·용량 검사
- `package_source.py`: 빌드 결과를 제외한 소스 ZIP
- `package_github.py`: Git에 추적된 파일만 GitHub용 ZIP으로 패키징
- `verify_checksums.py`: 풀어 놓은 소스 ZIP의 누락·변조 검사
- `verify_release_tag.py`: Git 태그와 중앙 버전 일치 검사
- `write_release_checksums.py`: 배포 파일 SHA-256 목록 생성
- `publish_github.ps1/.sh`: 안전한 첫 비공개 저장소 생성·업로드
- `make_universal.py`: 두 Mach-O 결합
- `make_icns.py`: PNG 기반 ICNS 생성

## `build`와 `dist`

- `build`: 삭제해도 되는 컴파일 중간 파일
- `dist`: 사용자에게 전달할 완성 결과

소스 수정은 이 두 폴더에서 하지 않습니다.
