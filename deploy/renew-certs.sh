#!/bin/sh
# Renew the Let's Encrypt certificate (one cert, four names) with uacme, then
# hand it to lighttpd and UnrealIRCd. Runs daily from uogradnet-renew.timer;
# uacme only talks to Let's Encrypt when the cert is within 30 days of expiry.
#
#   renew-certs.sh                 renew if due, install if renewed
#   renew-certs.sh --install-only  just copy the current cert to UnrealIRCd
set -eu

NAMES="uogradnet.xyz irc.uogradnet.xyz cs.uogradnet.xyz chat.uogradnet.xyz"
CERT=/etc/ssl/uacme/uogradnet.xyz/cert.pem
KEY=/etc/ssl/uacme/private/uogradnet.xyz/key.pem
TLS=/home/unrealircd/unrealircd/conf/tls

install_unreal() {
	install -o unrealircd -g unrealircd -m 644 "$CERT" "$TLS/server.cert.pem"
	install -o unrealircd -g unrealircd -m 600 "$KEY"  "$TLS/server.key.pem"
}

if [ "${1:-}" = --install-only ]; then
	install_unreal
	exit 0
fi

set +e
uacme -t EC -h /usr/share/uacme/uacme.sh issue $NAMES
rc=$?
set -e
case $rc in
	0)  echo "certificate renewed"
	    install_unreal
	    systemctl reload lighttpd
	    runuser -u unrealircd -- /home/unrealircd/unrealircd/unrealircd reloadtls ;;
	1)  echo "certificate not due for renewal" ;;
	*)  echo "uacme failed (exit $rc)" >&2; exit "$rc" ;;
esac
