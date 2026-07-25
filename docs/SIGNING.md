# 코드서명과 공증

## 원칙

소스 코드에 “서명됨” 표시를 넣는 것으로 운영체제 신뢰가 생기지 않습니다.
최종 빌드 바이트를 신원 확인된 인증서의 개인키로 서명해야 합니다.

인증서 개인키, PFX 비밀번호, Apple 계정 비밀번호는 소스 저장소나
스크립트에 넣지 않습니다.

## Windows

필요:

- 신뢰 가능한 Authenticode 코드서명 인증서 또는 Microsoft Trusted Signing
- Windows SDK의 `signtool.exe`

일반적인 서명 형태:

```powershell
signtool sign /fd SHA256 /tr <RFC3161_TIMESTAMP_URL> /td SHA256 `
  /a dist\Primbyul_v1.5.exe
```

검증:

```powershell
signtool verify /pa /v dist\Primbyul_v1.5.exe
Get-AuthenticodeSignature dist\Primbyul_v1.5.exe
```

자체서명 인증서는 테스트에는 쓸 수 있지만 다른 PC의 Smart App Control
신뢰를 자동으로 만들지 않습니다.

## macOS

필요:

- Apple Developer Program
- Developer ID Application 인증서
- `codesign`, `notarytool`, `stapler`

개념적 순서:

```bash
codesign --force --deep --options runtime \
  --sign "Developer ID Application: ..." \
  dist/Primbyul_v1.5_macOS/Primbyul.app

ditto -c -k --keepParent \
  dist/Primbyul_v1.5_macOS/Primbyul.app \
  Primbyul-notary.zip

xcrun notarytool submit Primbyul-notary.zip --wait --keychain-profile <PROFILE>
xcrun stapler staple dist/Primbyul_v1.5_macOS/Primbyul.app
codesign --verify --deep --strict --verbose=2 dist/Primbyul_v1.5_macOS/Primbyul.app
spctl --assess --type execute --verbose=4 dist/Primbyul_v1.5_macOS/Primbyul.app
```

공증 후 사용자 전달용 ZIP을 다시 생성합니다.

## CI에서 서명할 때

- 인증서는 암호화된 secret storage에 보관
- PR 빌드는 서명하지 않음
- 보호된 release tag에서만 서명
- 서명 로그에 비밀번호·개인키 경로 출력 금지
- 서명 후 해시와 검증 결과 기록
