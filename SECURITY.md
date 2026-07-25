# Security policy

## Supported version

Only the newest tagged release is maintained.

## Reporting

Do not put credentials, signing certificates, private photos, Windows registry
exports, or personal paths in a public issue.

For a private repository, use a private issue or repository security advisory.
For a public repository, enable **Settings → Security → Private vulnerability
reporting** before accepting external reports.

## Release trust

GitHub Actions produces unsigned build candidates. Windows Authenticode signing
and Apple Developer ID signing/notarization require owner-controlled credentials
and must be performed as a separate protected release step.
