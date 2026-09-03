#!/usr/bin/env bash
set -euo pipefail

version=${1:-}
if [[ ! $version =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "usage: ./scripts/publish-stable.sh vMAJOR.MINOR.PATCH" >&2
    exit 2
fi

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
git diff --quiet && git diff --cached --quiet || { echo "commit all changes before publishing stable" >&2; exit 1; }
git remote get-url stable >/dev/null 2>&1 || { echo "missing 'stable' remote (public Vvoidddd/Axiom)" >&2; exit 1; }

./all
if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    ./tests/smoke.sh axiom.iso
else
    echo "warning: QEMU is unavailable; stable runtime tests were not run here" >&2
fi

tag="stable-$version"
git rev-parse "$tag" >/dev/null 2>&1 && { echo "tag $tag already exists" >&2; exit 1; }
git tag -a "$tag" -m "Axiom stable $version"
git push stable HEAD:main "$tag"
echo "Pushed stable source and $tag. GitHub Actions will attach axiom.iso to the Release."
