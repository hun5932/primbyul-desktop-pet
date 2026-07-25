# 문제 해결

## `Zig was not found`

```powershell
zig version
```

명령이 없으면 Zig 실행파일을 PATH에 추가하거나:

```powershell
$env:ZIG = "C:\tools\zig\zig.exe"
```

## `ReadOnlyFileSystem` 또는 Zig 캐시 오류

쓰기 가능한 캐시 폴더를 지정합니다.

Windows:

```powershell
$env:ZIG_LOCAL_CACHE_DIR = "$PWD\build\zig-local"
$env:ZIG_GLOBAL_CACHE_DIR = "$PWD\build\zig-global"
```

macOS/Linux:

```bash
export ZIG_LOCAL_CACHE_DIR="$PWD/build/zig-local"
export ZIG_GLOBAL_CACHE_DIR="$PWD/build/zig-global"
```

## RC에서 PNG 또는 ICO를 못 찾음

`src/windows/primbyul.rc`의 경로는 RC 파일 위치를 기준으로 합니다.

```text
../../assets/frames/...
../../assets/icons/Primbyul.ico
```

프로젝트 내부 폴더 구조를 바꿨다면 RC 경로도 같이 바꿔야 합니다.

## `SOURCE CHECK FAILED: version mismatch`

여러 파일을 직접 고친 상태입니다. 수동으로 부분 수정하지 말고:

```powershell
python tools\bump_version.py 1.6.0
python tools\validate_source.py
```

## 이미지 파일이 잘렸거나 0바이트

`build_assets.py`는 PNG를 메모리에서 완전히 인코딩한 뒤 한 번에 기록합니다.
그래도 실패하면 디스크 공간과 파일 잠금 여부를 확인하고 다시 실행합니다.

```powershell
python tools\build_assets.py
python tools\validate_source.py
```

## Windows에서 Smart App Control 차단

컴파일 오류가 아니라 신뢰 정책입니다. 보안을 끄는 것을 기본 해결책으로
삼지 마세요. 공인 Authenticode 서명을 적용하는 것이 배포 해법입니다.

## Mac에서 “개발자를 확인할 수 없음”

서명·공증되지 않은 개발 빌드는 앱을 Control-클릭하고 `열기`로 최초 실행할
수 있습니다. 공개 배포는 Developer ID 서명과 공증을 적용합니다.

## Mac 자동 실행 실패

1. 앱을 `/Applications`에 이동
2. macOS 13 이상인지 확인
3. 시스템 설정 → 일반 → 로그인 항목 확인
4. 메뉴에서 자동 실행을 껐다 다시 켜기

## 클릭 통과를 켜서 펫을 조작할 수 없음

- Windows: 작업표시줄 알림 영역의 프림별 아이콘
- macOS: 메뉴 막대의 프림별 아이콘

에서 `마우스 클릭 통과`를 해제합니다.

## 펫이 화면 밖으로 나감

메뉴에서 `오른쪽 아래로 이동`을 선택합니다. 앱은 디스플레이/DPI 변경 시
위치를 작업영역 안으로 자동 보정합니다.
