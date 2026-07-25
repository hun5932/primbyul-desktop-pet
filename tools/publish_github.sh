#!/usr/bin/env bash
set -euo pipefail

repository_name="${1:-primbyul-desktop-pet}"
visibility="${2:-private}"
public_confirmation="${3:-}"

if [[ "$visibility" != "private" && "$visibility" != "public" ]]; then
    echo "Visibility must be private or public." >&2
    exit 2
fi
if [[ "$visibility" == "public" && "$public_confirmation" != "--allow-public" ]]; then
    echo "Public upload requires: public --allow-public" >&2
    exit 2
fi

for command_name in git gh python3; do
    command -v "$command_name" >/dev/null 2>&1 || {
        echo "Required command is missing: $command_name" >&2
        exit 1
    }
done

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_root"

gh auth status
if [[ ! -d .git ]]; then
    git init -b main
fi

if [[ -z "$(git config user.name)" || -z "$(git config user.email)" ]]; then
    echo "Configure git user.name and user.email before publishing." >&2
    exit 1
fi

git add --all
# set -e already aborts if either validator exits non-zero (privacy/secret gate).
python3 tools/validate_github.py
python3 tools/validate_source.py

if ! git diff --cached --quiet; then
    git commit -m "Initial import: Primbyul v1.5"
fi

if git remote get-url origin >/dev/null 2>&1; then
    echo "An origin remote already exists; refusing to overwrite it." >&2
    exit 1
fi

gh repo create "$repository_name" "--$visibility" \
    --source . \
    --remote origin \
    --push \
    --description "A fluffy Samoyed that trots across your desktop - a native Windows and macOS desktop pet, hand-keyframed and tiny."

echo "Uploaded: $repository_name ($visibility)"
gh repo view --web
