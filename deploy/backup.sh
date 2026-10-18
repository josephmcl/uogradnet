#!/bin/sh
# Nightly backup of the data that is NOT in git: the chat log, Atheme's
# account/channel database and UnrealIRCd's channel/ban databases.
# Keeps 14 days in /var/backups/uogradnet. Copy these off the VM too.
set -eu

DEST=/var/backups/uogradnet/$(date +%F)
install -d -m 700 "$DEST"

cd /tmp
runuser -u postgres -- pg_dump -Fc chatlog > "$DEST/chatlog.dump"
cp /var/lib/atheme/services.db "$DEST/atheme-services.db"
cp /home/unrealircd/unrealircd/data/*.db "$DEST/" 2>/dev/null || true

find /var/backups/uogradnet -mindepth 1 -maxdepth 1 -type d -mtime +14 -exec rm -rf {} +
echo "backup written to $DEST"
