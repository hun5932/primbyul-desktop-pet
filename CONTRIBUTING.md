# Contributing

## Before changing code

1. Read `docs/PROJECT-STRUCTURE.md` and `docs/CODE-WALKTHROUGH.md`.
2. Keep platform behavior aligned unless the change is intentionally
   platform-specific.
3. Do not commit original reference photos, credentials, certificates, or
   generated files from `build/` and `dist/`.

## Required checks

```bash
python tools/validate_github.py
python tools/validate_source.py
python tools/build_all.py
```

Behavior changes also require the manual Windows/macOS checks in
`docs/VERSION-AND-RELEASE.md`.

## Change scope

- One functional change per pull request.
- Explain the failure mode being fixed.
- Add an acceptance test that another person can repeat.
- Update `CHANGELOG.md` for user-visible changes.
- Use `tools/bump_version.py` instead of editing version strings separately.

## Assets

Generated frames may be changed only as a coherent animation set. Preserve
512×512 RGBA dimensions and transparent padding, then run
`tools/build_assets.py` and all validators.
