# 코드 설명

## 전체 흐름

```text
프로그램 시작
→ 저장 설정 읽기
→ 현재 모습·동작의 네 프레임 로드
→ 투명 펫 창과 제어 메뉴 생성
→ 25ms 스케줄러
→ 프레임 전환·눈 깜빡임·화면 이동
→ 위치와 설정 저장
```

Windows와 macOS는 UI 구현은 다르지만 상태 이름, 프레임 수, 모드,
타이밍 원칙을 동일하게 유지합니다.

## 공통 상태

| 상태 | 역할 | 키프레임 사용 |
| --- | --- | --- |
| `STATE_IDLE` | 서서 쉬기·눈 깜빡임 | 0, 반눈 1, 감은 눈 2 |
| `STATE_RUN_RIGHT` | 오른쪽 달리기 | 0→1→2→3 |
| `STATE_RUN_LEFT` | 왼쪽 달리기 | 오른쪽 미러 |
| `STATE_WAG` | 꼬리 흔들기 | 0→1→2→3 |
| `STATE_PLAY` | 플레이바우·점프 | 0→1→2→3→2→1→0 |
| `STATE_WATCH` | 고개 방향 변화 | 0→1→2→1→3→1→0→1 |
| `STATE_SIT` | 앉아서 쉬기·눈 깜빡임 | 0, 반눈 1, 감은 눈 2 |

## Windows: `src/windows/primbyul.c`

### 상수와 상태 테이블

- `STATE_*`: 실행 동작
- `MODE_*`: 자동, 앉기, 조용한 자동
- `APPEARANCE_*`: 성견, 퍼피
- `k_states`: 프레임 순서, 지연, 반복 횟수

새 행동의 속도를 바꾸려면 우선 `k_states`를 수정합니다. 타이머 자체를
바꾸면 모든 행동과 눈 깜빡임 정밀도에 영향을 주므로 피합니다.

### 이미지 리소스

`DecodeFrameResource`는 EXE의 PNG 리소스를 GDI+로 읽어 premultiplied
BGRA 버퍼로 만듭니다.

`LoadAnimationFrames`는 새 동작의 네 장을 임시 버퍼에 모두 성공적으로
읽은 뒤 이전 프레임과 교체합니다. 중간에 실패하면 기존 프레임을 유지하므로
부분 로드 상태가 생기지 않습니다.

`ResourceIdForFrame`은 모습·상태·키 번호를 RC 리소스 ID로 바꿉니다.
이 매핑을 수정하면 `primbyul.rc`도 반드시 같은 규칙으로 바꿔야 합니다.

### 렌더링

`CreateFrameSurface`가 32비트 DIB surface를 만듭니다.

`RenderFrame`은:

1. 현재 상태의 키 번호 선택
2. 필요하면 왼쪽 달리기 수평 반전
3. 512×512 원본을 현재 표시 크기로 bilinear 축소
4. `UpdateLayeredWindow`로 알파 포함 화면 갱신

원본은 프레임마다 같은 512×512 셀을 사용하므로 창 크기가 흔들리지 않습니다.

### 애니메이션 스케줄러

Windows 타이머는 25ms마다 `TickAnimation`을 호출하지만 실제 프레임 변경은
각 상태의 `delay_ms`가 지났을 때만 수행합니다.

`TickBlink`는 반눈→감은 눈→반눈→정면 순서이며 다음 깜빡임을 3~8초
사이로 다시 예약합니다.

`SelectNextState`는 현재 모드에 따라 다음 동작 확률을 선택합니다.

- 자동: 달리기 포함
- 조용히: 달리기 제외, 휴식과 지켜보기 비중 증가
- 앉기: 수동 동작이 끝나면 다시 앉기

### 창과 마우스

- `WM_NCHITTEST`: 투명 픽셀은 클릭을 뒤 프로그램으로 전달
- `WM_MOUSEACTIVATE`: 일반 클릭으로 작업 중인 앱의 포커스를 빼앗지 않음
- 드래그: 위치 이동 후 작업영역 안으로 보정
- 더블클릭: 꼬리 흔들기
- 우클릭: 전체 메뉴

### 설정과 자동 실행

설정 파일:

```text
%APPDATA%\Primbyul\settings.ini
```

자동 실행:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run
```

관리자 권한은 사용하지 않습니다. `SetAutostart`는 현재 EXE의 절대 경로를
따옴표로 감싸 등록합니다.

### 트레이 복구

`TaskbarCreated` 메시지를 등록해 Windows Explorer 재시작 후 알림 영역
아이콘을 다시 추가합니다. 초기 추가 실패 시 재시도 타이머를 사용합니다.

## Windows: `primbyul.rc`

RC에는 다음이 있습니다.

- 앱 아이콘
- manifest
- 성견 24장
- 퍼피 24장
- 파일/제품 버전 정보

리소스 ID 규칙:

```text
성견: 1100번대
퍼피: 2100번대
끝자리: 키프레임 0~3
```

Windows는 `run-left` 이미지를 넣지 않고 런타임에 반전합니다.

## macOS: `src/macos/primbyul_mac.c`

### AppKit 로딩 방식

macOS 코드는 AppKit과 ServiceManagement를 `dlopen`하고 Objective-C
런타임 함수를 동적으로 가져옵니다. 이 선택 덕분에 macOS SDK가 없는
Windows/Linux에서도 Zig로 Mach-O를 만들 수 있습니다.

대가로 `objc_msgSend` 호출형을 정확히 유지해야 합니다. 구조체 인자,
구조체 반환, 정수, 실수, 객체는 ABI가 다르므로 기존 typed wrapper와
호출 패턴을 복사해 사용합니다.

### 런타임 클래스

`register_runtime_classes`가 두 클래스를 만듭니다.

- `PrimbyulController`: 타이머와 메뉴 action
- `PrimbyulPetView`: 클릭, 더블클릭, 드래그, 우클릭, 알파 hit-test

메서드를 추가할 때 구현 함수, selector 이름, Objective-C type encoding을
함께 등록해야 합니다.

### 창

Dock 아이콘이 없는 non-activating `NSPanel`을 사용합니다.

- 투명 배경
- 항상 위 레벨 선택 가능
- 모든 Space에 표시
- 클릭 통과 가능
- 프레임 알파가 낮은 영역은 hit-test에서 제외

### 프레임

`load_frames`는 앱 번들의 다음 위치에서 현재 네 장만 읽습니다.

```text
Contents/Resources/frames/<adult|puppy>/<action>/<0..3>.png
```

Mac은 왼쪽 달리기 PNG를 미리 만들어 사용합니다.

### 메뉴 막대

`build_menu`가 모든 설정을 한 메뉴에 구성합니다. 클릭 통과나 숨기기를
사용해도 메뉴 막대 아이콘은 남아 복구 경로가 유지됩니다.

### 설정과 로그인 자동 실행

설정은 `NSUserDefaults`의 번들 도메인에 저장됩니다.

로그인 자동 실행은 `SMAppService.mainAppService`를 사용합니다.
macOS 13 미만에서는 지원 안내를 표시합니다.

## 종료와 제거

양쪽 모두 일반 종료 시 설정을 저장합니다.

`설정 제거 후 종료`는 자동 실행 등록과 저장 설정을 제거하지만 프로그램
파일 자체는 삭제하지 않습니다. 이후 EXE 또는 `.app`을 사용자가 삭제하면
완전 제거됩니다.
