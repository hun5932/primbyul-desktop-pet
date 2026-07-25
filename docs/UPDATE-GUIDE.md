# 수정·업데이트 가이드

## 코드만 수정

예: 동작 확률, 속도, 메뉴 문구

1. 플랫폼 코드를 수정합니다.
2. 다른 플랫폼도 동일한 의미가 필요한지 확인합니다.
3. 전체 빌드합니다.

```powershell
python tools\build_all.py
```

4. Windows와 Mac 실제 기기에서 수동 테스트합니다.
5. 변경 내용을 `CHANGELOG.md`에 기록합니다.

## 프레임만 수정

1. `assets/source-keyframes`의 해당 네 장을 교체합니다.
2. 프레임 생성 도구를 실행합니다.
3. 전체 빌드합니다.

```powershell
python tools\build_assets.py
python tools\build_all.py
```

완성 프레임인 `assets/frames`만 직접 수정할 수도 있지만 다음번
`build_assets.py` 실행 시 덮어써집니다. 유지할 수정은 반드시
`source-keyframes`에도 반영합니다.

## 앱 아이콘 수정

1. `assets/icons/primbyul-icon-master.png` 교체
2. `python tools\build_assets.py`
3. `python tools\build_all.py`

도구가 Windows ICO, macOS용 1024 PNG, 메뉴 막대 PNG를 함께 갱신합니다.

## 동작 속도 수정

Windows:

```text
src/windows/primbyul.c
→ k_states
```

macOS:

```text
src/macos/primbyul_mac.c
→ FRAME_DELAYS, LOOP_COUNTS
```

두 값의 단위:

- Windows: 밀리초
- macOS: 초

## 자동 행동 비율 수정

두 플랫폼의 `select/SelectNextState`를 같은 확률로 수정합니다.

확률 합을 별도로 100으로 적는 방식이 아니라 누적 경계값을 사용하므로
경계의 순서를 깨뜨리지 않습니다.

## 설정 항목 추가

필요한 작업:

1. 전역 상태 추가
2. 설정 읽기 기본값 정의
3. 저장 함수에 추가
4. 메뉴 또는 설정 창 control 추가
5. command/action handler 추가
6. 기존 설정 파일에 값이 없을 때의 fallback 검증
7. 설정 제거 기능이 새 저장값도 제거하는지 확인

## 버전 올리기

```powershell
python tools\bump_version.py 1.6.0
```

자동 변경:

- `config/project.json`
- Windows VERSIONINFO
- Windows manifest
- Windows 원본 파일명
- macOS Info.plist
- macOS 메뉴 버전

수동 변경:

- `CHANGELOG.md`
- 필요하면 문서의 기능 설명

그 다음:

```powershell
python tools\build_all.py
python tools\package_source.py
```

## 변경을 되돌릴 때

Git을 사용하는 것이 가장 안전합니다.

```powershell
git init
git add .
git commit -m "Primbyul v1.5 source baseline"
```

각 기능 수정 전 새 commit 또는 branch를 만들면 이미지·코드·문서 변경을
함께 비교하고 되돌릴 수 있습니다. `build/`와 `dist/`는 `.gitignore`로
제외됩니다.
