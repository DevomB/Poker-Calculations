#!/usr/bin/env bash
# Regenerate package-lock.json from package.json and push if it changed.
# Used by .github/actions/sync-package-lock. Requires git repo with push access.
# Env: COMMIT_AUTHOR_NAME, COMMIT_AUTHOR_EMAIL (required when committing).
set -euo pipefail

npm install --package-lock-only
git add package-lock.json

if git diff --staged --quiet; then
  echo "package-lock.json already matches package.json"
  exit 0
fi

git config user.name "${COMMIT_AUTHOR_NAME:?COMMIT_AUTHOR_NAME is required to commit}"
git config user.email "${COMMIT_AUTHOR_EMAIL:?COMMIT_AUTHOR_EMAIL is required to commit}"
git commit -m "chore: sync package-lock.json"
git push
