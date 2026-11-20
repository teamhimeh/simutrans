#!/bin/bash
# Verify that a triggering tag still names the reported commit and that the
# commit is reachable from the trusted OTRP-KUTAv6 branch. Run from a full
# checkout of this repository, before any signing secrets are available.
set -euo pipefail

tag=${1:?tag name required}
commit=${2:?commit SHA required}
branch_ref=${3:-refs/remotes/origin/OTRP-KUTAv6}

case "$tag" in v*) ;; *) echo "Not a v tag" >&2; exit 1 ;; esac
git check-ref-format "refs/tags/$tag"
if ! [[ "$commit" =~ ^[0-9a-f]{40}$ ]]; then
  echo "Invalid triggering commit SHA" >&2
  exit 1
fi
git rev-parse --verify "$branch_ref^{commit}" >/dev/null
resolved=$(git rev-parse --verify "refs/tags/$tag^{commit}")
if [ "$resolved" != "$commit" ]; then
  echo "Tag no longer resolves to the triggering commit" >&2
  exit 1
fi
if git merge-base --is-ancestor "$commit" "$branch_ref"; then
  echo "eligible=true"
else
  echo "eligible=false"
fi
