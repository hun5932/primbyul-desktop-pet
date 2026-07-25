# Primbyul

**[English](README.md) · [한국어](README.ko.md)**

A native desktop pet Samoyed that trots across your screen — Windows and macOS.

[![CI](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)

Prim comes in two looks — a grown-up Samoyed and a puppy — and wanders across
your desktop using hand-drawn 512×512 keyframes. She runs, wags her tail, plays,
watches from the side, sits, and blinks naturally. She runs as a small
single-file executable with no extra runtime, and every setting lives in the
notification-area (tray) icon and the menu bar.

This repository holds the full source for editing, building, and updating
Primbyul.

> Privacy-safe defaults for a public repository are in place: Prim's real
> reference photos, certificates, private keys, and build output are never
> committed to Git.

## Features

- **Two looks**: adult Prim and puppy Prim
- **Actions**: running, tail-wagging, playing, watching from the side, sitting, plus natural eye blinking
- **Modes**: play automatically, watch quietly (no running), or sit calmly
- **On-screen control**: mouse click-through, always-on-top, size presets (75 / 100 / 135%), drag to move, remembered position, multi-monitor recovery
- **Windows**: run at login, notification-area icon recovery after Explorer restarts, native single-file GUI EXE
- **macOS**: Intel and Apple Silicon Universal app, menu-bar control, launch at login
- **Lightweight**: only the four frames of the current action are held in memory

## Screenshot

<p align="center">
  <img src="assets/frames/adult/wag/0.png" alt="Primbyul" width="220">
</p>

## Get it and run it

For now the primary path is **building from source** (see "Quick start" below).
Signed and notarized release builds are published as GitHub Releases drafts once
a version tag is pushed. See [VERSION-AND-RELEASE.md](docs/VERSION-AND-RELEASE.md)
for details.

## Design principles

- Platform code and image assets are kept separate.
- Build output is generated only under `dist/`.
- The version is changed in one place, `config/project.json`.
- Windows and macOS share the same 512×512 frames.
- Sources, frames, executable format, and ZIP permissions are checked automatically before release.

## Quick start

Requirements:

1. Python 3.11 or newer
2. The Zig compiler

Extract the archive into a folder such as `C:\workspace\Primbyul`, then in a
terminal:

```powershell
cd C:\workspace\Primbyul
python tools\build_all.py
```

If Zig is not on PATH, you can point to the executable for the current terminal:

```powershell
$env:ZIG = "C:\tools\zig\zig.exe"
python tools\build_all.py
```

Output:

```text
dist/
├─ Primbyul_v1.5.exe
├─ Primbyul_v1.5_macOS/
└─ Primbyul_v1.5_macOS.zip
```

Windows only:

```powershell
python tools\build_windows.py
```

macOS only:

```powershell
python tools\build_macos.py
```

## Making a source backup ZIP

This bundles only source code, docs, and original images, excluding `build/`,
`dist/`, caches, and the EXE.

```powershell
python tools\package_source.py
```

The generated source ZIP includes `SOURCE-CHECKSUMS.sha256`, which records the
SHA-256 of every file. After extracting the ZIP again, you can check for missing
or corrupted files with:

```powershell
python tools\verify_checksums.py
python tools\validate_source.py
```

## Making the next version

```powershell
python tools\bump_version.py 1.6.0
python tools\build_all.py
```

`bump_version.py` updates the Windows resource and manifest, the macOS
Info.plist and menu label, and the central config together. After that, record
the changes yourself in `CHANGELOG.md`.

## Editing the animation

1. Replace `assets/source-keyframes/<appearance>/<action>/key-0.png` through
   `key-3.png`.
2. Install the image-tool dependencies.

```powershell
python -m pip install -r requirements-assets.txt
python tools\build_assets.py
python tools\build_all.py
```

Read [ANIMATION-WORKFLOW.md](docs/ANIMATION-WORKFLOW.md) first for the detailed
rules.

## Documentation map

- [BUILD.md](docs/BUILD.md): setup, building, output
- [PROJECT-STRUCTURE.md](docs/PROJECT-STRUCTURE.md): what each folder does
- [CODE-WALKTHROUGH.md](docs/CODE-WALKTHROUGH.md): Windows/macOS code explained
- [UPDATE-GUIDE.md](docs/UPDATE-GUIDE.md): procedures by change type
- [ANIMATION-WORKFLOW.md](docs/ANIMATION-WORKFLOW.md): frame creation and replacement rules
- [VERSION-AND-RELEASE.md](docs/VERSION-AND-RELEASE.md): version and release management
- [GITHUB.md](docs/GITHUB.md): repository creation, automated builds, tag releases
- [SIGNING.md](docs/SIGNING.md): Windows signing, Mac signing/notarization
- [KNOWN-LIMITATIONS.md](docs/KNOWN-LIMITATIONS.md): current limits and caveats
- [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md): build and run problems

## Key facts

- The current source builds Windows x64 and macOS Universal (x86_64 + arm64).
- The Windows EXE is a native GUI that does not launch a separate PowerShell/BAT.
- The Mac app can restore every setting from the menu bar.
- The build scripts do not include a code-signing certificate.
- Original reference photos are excluded from Git and are not needed to run or build.
- No public reuse license is set for the artwork; the Prim images are personal
  and not for reuse. Source code is under the MIT License. See `LICENSE`.
- Before an actual release, you must run the manual smoke tests in
  `docs/VERSION-AND-RELEASE.md` on each operating system.
