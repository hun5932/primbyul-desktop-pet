# GitHub 저장소 운영 가이드

## 1. 기본 원칙

첫 업로드는 비공개 저장소로 시작합니다. 현재 저장소에는 실제 실행용
프레임이 포함되지만 다음 항목은 Git에서 제외됩니다.

- 실제 프림 참고사진
- Windows·Apple 코드서명 인증서와 개인키
- `.env`와 `secrets/`
- `build/`, 완성 EXE·ZIP, 컴파일 캐시

`tools/validate_github.py`가 Git에 추적되는 파일을 기준으로 이 규칙과
GitHub의 파일당 100 MiB 제한을 검사합니다.

## 2. 권장 방법: GitHub CLI

필수 프로그램:

1. Git
2. GitHub CLI (`gh`)
3. Python 3.11 이상

먼저 한 번 로그인합니다.

```powershell
gh auth login
```

브라우저 로그인 방식을 선택하고, 프로젝트 폴더에서 다음을 실행합니다.

Windows:

```powershell
.\tools\publish_github.ps1
```

Mac/Linux:

```bash
./tools/publish_github.sh
```

기본값은 `Primbyul`이라는 **비공개** 저장소입니다. 이름을 바꾸려면:

```powershell
.\tools\publish_github.ps1 -RepositoryName Primbyul-Desktop-Pet
```

```bash
./tools/publish_github.sh Primbyul-Desktop-Pet private
```

공개 저장소는 의도하지 않은 공개를 막기 위해 확인 인수가 하나 더
필요합니다.

```powershell
.\tools\publish_github.ps1 `
  -RepositoryName Primbyul-Desktop-Pet `
  -Visibility public `
  -AllowPublic
```

```bash
./tools/publish_github.sh Primbyul-Desktop-Pet public --allow-public
```

스크립트는 기존 `origin` 원격 저장소를 덮어쓰지 않습니다. 이미 원격이
있다면 일반 Git 명령으로 업데이트합니다.

## 3. GitHub Desktop

명령줄을 쓰고 싶지 않을 때 사용할 수 있습니다.

1. GitHub Desktop에서 **Add an Existing Repository from your Hard Drive**
2. 이 폴더 선택
3. Git 저장소가 아니라는 안내가 나오면 **create a repository**
4. 변경 파일 목록에서 `assets/reference-photos/*.png`, `build/`, `dist/`
   결과물이 없는지 확인
5. 첫 커밋 생성
6. **Publish repository**
7. 처음에는 **Keep this code private**를 켠 상태로 게시

GitHub 웹 업로드는 한 번에 100개 파일 제한이 있어 이 프로젝트의 전체
업로드 방법으로는 적합하지 않습니다.

## 4. 평소 업데이트

```powershell
git switch -c feature/short-description
# 코드 또는 프레임 수정
python tools\validate_github.py
python tools\build_all.py
git add .
git commit -m "Describe the change"
git push -u origin feature/short-description
```

GitHub에서 Pull Request를 만들고 CI가 통과한 뒤 `main`에 병합합니다.

## 5. GitHub Actions

### CI

`.github/workflows/ci.yml`은 `main` push, Pull Request, 수동 실행에서:

1. GitHub 개인정보·추적 파일 검사
2. 버전·프레임·리소스 검사
3. Windows x64 EXE 크로스 빌드
4. macOS x86_64 + arm64 Universal 앱 크로스 빌드
5. 바이너리와 ZIP 구조 검사
6. 14일 동안 내려받을 수 있는 Actions artifact 생성

빌드는 검증된 Zig
`0.14.0-dev.2456+a68119f8f`로 고정되어 있습니다. Zig 설치 Action도
검토한 커밋 SHA에 고정되어 있으며 Dependabot이 Actions 업데이트를
제안합니다.

### 자동 릴리스

`vMAJOR.MINOR.PATCH` 태그가 중앙 버전과 정확히 일치할 때만 실행됩니다.

```powershell
python tools\bump_version.py 1.6.0
# CHANGELOG.md 작성
python tools\build_all.py
git add .
git commit -m "Release 1.6.0"
git tag v1.6.0
git push origin main
git push origin v1.6.0
```

Actions는 EXE, macOS ZIP, 소스 ZIP, `SHA256SUMS.txt`를 첨부한
**초안(Draft) Release**를 만듭니다. 자동 공개하지 않는 이유는 실제
Windows·Mac 테스트와 코드서명을 완료한 뒤 사람이 최종 승인해야 하기
때문입니다.

## 6. 코드서명

인증서를 저장소에 커밋하지 않습니다. 현재 Actions 결과는 빌드 검사용
미서명 파일입니다.

공개 배포본은 `SIGNING.md` 절차에 따라 보호된 로컬 또는 별도의 제한된
서명 환경에서 서명·공증한 뒤 초안 Release의 첨부 파일을 교체합니다.

## 7. 공개 저장소 전환 전 결정

현재 프로젝트에는 오픈소스 라이선스가 없습니다. 라이선스가 없으면
기본 저작권이 적용되어 다른 사람에게 복제·수정·재배포 권한을 주지
않습니다.

공개 목적에 따라 하나를 명시적으로 선택하세요.

- 코드 공개만 하고 재사용을 허용하지 않음: 라이선스 미지정 유지
- 자유로운 수정·재배포 허용: MIT 또는 Apache-2.0 검토
- 수정본도 같은 라이선스로 공개: GPL 계열 검토

애니메이션 프레임과 캐릭터 자산을 코드와 같은 조건으로 공개할지도
별도로 결정해야 합니다. 법적 의도가 정해지기 전 자동으로 MIT를 붙이지
않는 것이 안전합니다.

## 8. 저장소 설정 권장값

GitHub의 **Settings**에서:

- Default branch: `main`
- Branch protection: Pull Request와 CI 통과 요구
- Actions permissions: Read repository contents
- Release workflow만 `contents: write`
- Private vulnerability reporting 활성화
- Secret scanning과 push protection 사용 가능 시 활성화

현재 파일 중 50 MiB를 넘는 항목이 없으므로 Git LFS는 필요하지 않습니다.
프레임을 영상이나 대용량 원본으로 교체할 때만 LFS를 검토합니다.

## 9. 올리기 전 직접 확인

```powershell
git status --short
git status --ignored
python tools\validate_github.py
python tools\validate_source.py
```

`git status --ignored`에는 참고사진, `build/`, `dist/` 결과물이 보여도
정상입니다. `git status --short`의 커밋 대상에는 나타나면 안 됩니다.
