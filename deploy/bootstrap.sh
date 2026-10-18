#!/bin/sh
# One-time setup of a fresh Ubuntu 24.04 VM (arm64 or amd64) for UOGradNet.
#
#   sudo deploy/bootstrap.sh you@example.com yournick
#
# Installs packages, compiles UnrealIRCd, creates users and the database,
# generates secrets (printing the IRC operator password once), gets the
# Let's Encrypt certificate, then runs the first deploy. Safe to re-run:
# every step checks whether it is already done.
set -eu

UNREAL_VERSION=6.2.7
KIWI_VERSION=1.7.1
KIWI_BUILD=2
NAMES="uogradnet.xyz irc.uogradnet.xyz cs.uogradnet.xyz chat.uogradnet.xyz"

[ "$(id -u)" -eq 0 ] || { echo "run with sudo" >&2; exit 1; }
[ $# -eq 2 ] || { echo "usage: $0 ADMIN_EMAIL ADMIN_NICK" >&2; exit 64; }
EMAIL=$1
ADMIN_NICK=$2
SRC=$(cd "$(dirname "$0")/.." && pwd)
UHOME=/home/unrealircd
say() { printf '\n==> %s\n' "$*"; }

say "packages"
apt-get update -q
DEBIAN_FRONTEND=noninteractive apt-get install -y -q \
	build-essential pkg-config git make unzip curl \
	libssl-dev libpcre2-dev libargon2-dev libsodium-dev libc-ares-dev libcurl4-openssl-dev \
	libpq-dev postgresql lighttpd uacme msmtp xz-utils


say "users"
id unrealircd >/dev/null 2>&1 || adduser --disabled-password --gecos "" unrealircd
id chatlog    >/dev/null 2>&1 || adduser --system --group --no-create-home chatlog

say "UnrealIRCd $UNREAL_VERSION (compiling: about 10 minutes on 1 OCPU)"
if [ ! -x "$UHOME/unrealircd/bin/unrealircd" ] || \
   ! runuser -u unrealircd -- "$UHOME/unrealircd/bin/unrealircd" -v 2>/dev/null | grep -q "$UNREAL_VERSION"; then
	install -o unrealircd -g unrealircd -m 644 "$SRC/unrealircd/config.settings" "$UHOME/config.settings"
	runuser -u unrealircd -- sh -eu -c "
		cd $UHOME
		curl -fsSLo unrealircd-$UNREAL_VERSION.tar.gz https://www.unrealircd.org/downloads/unrealircd-$UNREAL_VERSION.tar.gz
		rm -rf unrealircd-$UNREAL_VERSION && tar xzf unrealircd-$UNREAL_VERSION.tar.gz
		cd unrealircd-$UNREAL_VERSION
		cp $UHOME/config.settings config.settings
		./Config -quick </dev/null
		make -j\$(nproc)
		make install"
fi
install -d -o unrealircd -g unrealircd -m 700 "$UHOME/unrealircd/conf/tls"

say "Atheme (compiling, about 3 minutes)"
sh "$SRC/deploy/atheme-build.sh"

say "PostgreSQL: chatlog database"
cd /tmp
runuser -u postgres -- psql -tAc "SELECT 1 FROM pg_roles WHERE rolname = 'chatlog'" | grep -q 1 ||
	runuser -u postgres -- psql -qc "CREATE ROLE chatlog LOGIN"
runuser -u postgres -- psql -tAc "SELECT 1 FROM pg_database WHERE datname = 'chatlog'" | grep -q 1 ||
	runuser -u postgres -- psql -qc "CREATE DATABASE chatlog OWNER chatlog"

say "Kiwi IRC $KIWI_VERSION (browser chat client)"
if [ ! -f /srv/uogradnet/www/chat/index.html ]; then
	tmp=$(mktemp -d)
	curl -fsSLo "$tmp/kiwi.zip" "https://github.com/kiwiirc/kiwiirc/releases/download/v$KIWI_VERSION/kiwiirc-client_v$KIWI_VERSION-${KIWI_BUILD}_any.zip"
	unzip -q "$tmp/kiwi.zip" -d "$tmp"
	install -d /srv/uogradnet/www/chat
	cp -r "$tmp"/kiwiirc_*/. /srv/uogradnet/www/chat/
	rm -rf "$tmp"
fi

say "secrets"
install -d -m 755 /etc/uogradnet
if [ ! -f /etc/uogradnet/secrets.env ]; then
	rnd() { openssl rand -base64 160 | tr -dc 'A-Za-z0-9' | cut -c1-"$1"; }
	OPER_PW=$(rnd 20)
	HASH=$(printf '%s\n' "$OPER_PW" | runuser -u unrealircd -- "$UHOME/unrealircd/unrealircd" mkpasswd 2>/dev/null | grep -o '\$argon2id[^ ]*')
	umask 077
	cat > /etc/uogradnet/secrets.env <<EOF
ADMIN_EMAIL=$EMAIL
OPER_PASSWORD_HASH=$HASH
CLOAK_KEY_1=$(rnd 80)
CLOAK_KEY_2=$(rnd 80)
CLOAK_KEY_3=$(rnd 80)
SERVICES_LINK_PASSWORD=$(rnd 40)
SERVICES_ADMIN_ACCOUNT=$ADMIN_NICK
CHATLOG_PASSWORD=$(rnd 32)
MAIL_FROM=noreply@uogradnet.xyz
SMTP_TOKEN=REPLACE_ME
EOF
	umask 022
	echo
	echo "  IRC operator password (shown once; log in with /OPER admin <password>):"
	echo
	echo "      $OPER_PW"
	echo
	echo "  Then put your Proton SMTP token in /etc/uogradnet/secrets.env"
	echo "  (SMTP_TOKEN; see etc/secrets.env.example) so registration emails work."
	echo
fi

say "firewall: 80, 443, 6767"
if command -v ufw >/dev/null && ufw status | grep -q '^Status: active'; then
	ufw allow 80,443,6767/tcp >/dev/null    # Vultr's Ubuntu image runs ufw
else
	for port in 80 443 6767; do
		iptables -C INPUT -p tcp --dport "$port" -m conntrack --ctstate NEW -j ACCEPT 2>/dev/null ||
			iptables -I INPUT -p tcp --dport "$port" -m conntrack --ctstate NEW -j ACCEPT
	done
	command -v netfilter-persistent >/dev/null && netfilter-persistent save >/dev/null
fi

say "TLS certificate for: $NAMES"
install -d /var/www/.well-known/acme-challenge
if [ ! -f /etc/ssl/uacme/uogradnet.xyz/cert.pem ]; then
	# lighttpd needs *a* certificate to start; this placeholder lives one day
	# and is replaced by the real one a moment later.
	install -d /etc/ssl/uacme/uogradnet.xyz /etc/ssl/uacme/private/uogradnet.xyz
	openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -days 1 \
		-subj /CN=uogradnet.xyz -keyout /etc/ssl/uacme/private/uogradnet.xyz/key.pem \
		-out /etc/ssl/uacme/uogradnet.xyz/cert.pem 2>/dev/null
	PLACEHOLDER=1
fi
install -m 644 "$SRC/lighttpd/lighttpd.conf" /etc/lighttpd/lighttpd.conf
install -d /srv/uogradnet/www/site
systemctl restart lighttpd
[ -f /etc/ssl/uacme/private/key.pem ] || uacme -y new "$EMAIL"
if [ "${PLACEHOLDER:-0}" = 1 ]; then
	uacme -f -t EC -h /usr/share/uacme/uacme.sh issue $NAMES
fi

say "first deploy"
make -C "$SRC" deploy

say "done"
echo "Next: README.md, 'First boot' (register your nick and the chatlog bot's)."
