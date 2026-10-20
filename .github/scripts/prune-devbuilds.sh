#!/usr/bin/env bash
# Keeps only the newest N devbuilds of every version line (default 15) and deletes the older ones (release + tag).
# Stable releases are never touched. Needs GH_TOKEN and GITHUB_REPOSITORY. DRY_RUN=1 only prints what would go.
set -euo pipefail
keep="${1:-15}"
old=$(gh release list --repo "$GITHUB_REPOSITORY" --limit 1000 --json tagName,isPrerelease \
  | jq -r --argjson keep "$keep" '
      [.[] | select(.isPrerelease) | .tagName
       | capture("^v(?<core>[0-9]+\\.[0-9]+\\.[0-9]+)-dev\\.(?<n>[0-9]+)$")
       | {tag: ("v" + .core + "-dev." + .n), core: .core, n: (.n | tonumber)}]
      | group_by(.core) | map(sort_by(-.n) | .[$keep:][]) | .[].tag')
if [ -z "$old" ]; then echo "Nothing to prune (keeping $keep per version line)."; exit 0; fi
for tag in $old; do
  if [ "${DRY_RUN:-0}" = "1" ]; then echo "would delete $tag"; continue; fi
  echo "deleting $tag"
  gh release delete "$tag" --repo "$GITHUB_REPOSITORY" --cleanup-tag --yes
done
