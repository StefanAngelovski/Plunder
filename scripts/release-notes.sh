#!/usr/bin/env bash
# Prints the GitHub release body for a version: the "## vX.Y.Z" section of
# CHANGELOG.md wrapped in .github/release-template.md.
#
#   scripts/release-notes.sh 2.3.0          # preview locally
#
# Exits non-zero if CHANGELOG.md has no section for that version, so a release
# can never go out without notes.
set -euo pipefail

version=${1:?usage: release-notes.sh <version, e.g. 2.3.0>}
root=$(cd "$(dirname "$0")/.." && pwd)
changelog="$root/CHANGELOG.md"
template="$root/.github/release-template.md"

section=$(mktemp)
trap 'rm -f "$section"' EXIT

# Lines after "## v<version>" up to the next "## " heading, trimmed of blank edges.
awk -v head="## v$version" '
    $0 == head || index($0, head " ") == 1 { found = 1; next }
    found && /^## / { exit }
    found { lines[++n] = $0 }
    END {
        first = 1; while (first <= n && lines[first] ~ /^[[:space:]]*$/) first++
        last = n;  while (last >= first && lines[last] ~ /^[[:space:]]*$/) last--
        for (i = first; i <= last; i++) print lines[i]
    }' "$changelog" > "$section"

if ! grep -q '[^[:space:]]' "$section"; then
    echo "error: CHANGELOG.md has no notes under '## v$version'." >&2
    echo "       Rename '## Unreleased' to '## v$version' (or add that section) before releasing." >&2
    exit 1
fi

awk -v version="$version" -v notes_file="$section" '
    $0 == "{{NOTES}}" { while ((getline line < notes_file) > 0) print line; next }
    { gsub(/\{\{VERSION\}\}/, version); print }' "$template"
