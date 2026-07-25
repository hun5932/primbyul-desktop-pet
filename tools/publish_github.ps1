[CmdletBinding()]
param(
    [string]$RepositoryName = "primbyul-desktop-pet",
    [ValidateSet("private", "public")]
    [string]$Visibility = "private",
    [switch]$AllowPublic
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($Visibility -eq "public" -and -not $AllowPublic) {
    throw "Public upload requires both -Visibility public and -AllowPublic."
}

foreach ($Command in @("git", "gh", "python")) {
    if (-not (Get-Command $Command -ErrorAction SilentlyContinue)) {
        throw "Required command is not installed or not on PATH: $Command"
    }
}

$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

gh auth status
if ($LASTEXITCODE -ne 0) {
    throw "GitHub CLI is not signed in. Run: gh auth login"
}

if (-not (Test-Path ".git")) {
    git init -b main
    if ($LASTEXITCODE -ne 0) { throw "git init failed" }
}

$UserName = git config user.name
$UserEmail = git config user.email
if (-not $UserName -or -not $UserEmail) {
    throw "Configure Git first: git config --global user.name and user.email"
}

git add --all
if ($LASTEXITCODE -ne 0) { throw "git add failed" }

# PowerShell's $ErrorActionPreference does NOT abort on a native command's
# non-zero exit. These validators guard against leaking Prim's reference photos
# or signing secrets, so their exit codes must be checked explicitly.
python tools/validate_github.py
if ($LASTEXITCODE -ne 0) { throw "validate_github.py failed; refusing to publish" }
python tools/validate_source.py
if ($LASTEXITCODE -ne 0) { throw "validate_source.py failed; refusing to publish" }

$StagedFiles = @(git diff --cached --name-only)
if ($StagedFiles.Count -gt 0) {
    git commit -m "Initial import: Primbyul v1.5"
    if ($LASTEXITCODE -ne 0) { throw "git commit failed" }
}

$RemoteNames = @(git remote)
if ($RemoteNames -contains "origin") {
    throw "An origin remote already exists. This script will not overwrite it."
}

$VisibilityFlag = "--$Visibility"
gh repo create $RepositoryName $VisibilityFlag --source . --remote origin --push `
    --description "A fluffy Samoyed that trots across your desktop - a native Windows and macOS desktop pet, hand-keyframed and tiny."
if ($LASTEXITCODE -ne 0) { throw "GitHub repository creation or push failed" }

Write-Host "Uploaded: $RepositoryName ($Visibility)"
gh repo view --web
