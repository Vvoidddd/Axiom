#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
git diff --quiet && git diff --cached --quiet || { echo "commit all changes before publishing daily" >&2; exit 1; }
git remote get-url origin >/dev/null 2>&1 || { echo "missing 'origin' remote (Vvoidddd/Axiom)" >&2; exit 1; }

tag="daily-$(date -u +%Y-%m-%d)"
if git rev-parse "$tag" >/dev/null 2>&1; then
    tag="$tag-$(date -u +%H%M%S)"
fi
git tag -a "$tag" -m "Axiom daily snapshot $(date -u +%F)"
git push origin HEAD:daily "$tag"
echo "Pushed daily source snapshot $tag to the daily branch."
