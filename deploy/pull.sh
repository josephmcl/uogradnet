#!/bin/sh
# Opt-in "git push to deploy": fetch origin/main and, if it moved, fast-forward
# and run `make deploy`. Enable with:
#   sudo systemctl enable --now uogradnet-pull.timer
# Anyone who can push to main can then change the server, so protect the branch.
set -eu
cd /srv/uogradnet/src
git fetch --quiet origin main
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] && exit 0
git merge --ff-only --quiet origin/main
echo "deploying $(git rev-parse --short HEAD): $(git log -1 --format=%s)"
make deploy
