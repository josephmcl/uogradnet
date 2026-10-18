#!/bin/sh
# Build Atheme IRC Services from the release tarball, with our
# nickserv/allowmail module (atheme/allowmail.c) compiled into its tree,
# and install it to /opt/atheme. The source tree stays in
# /usr/local/src/atheme so deploy.sh can rebuild just the module.
#
#   sudo sh deploy/atheme-build.sh          build + install (if needed)
#   sudo sh deploy/atheme-build.sh module   rebuild only allowmail
set -eu

VERSION=7.2.12
SRC=$(cd "$(dirname "$0")/.." && pwd)
TREE=/usr/local/src/atheme
NICKSERV=$TREE/modules/nickserv

[ "$(id -u)" -eq 0 ] || { echo "run with sudo" >&2; exit 1; }

module() {
	cp "$SRC/atheme/allowmail.c" "$NICKSERV/allowmail.c"
	cp "$SRC/atheme/allowmail.help" "$TREE/help/default/nickserv/allowmail"
	make -s -C "$NICKSERV"
	make -s -C "$NICKSERV" install
	install -m 644 "$SRC/atheme/allowmail.help" /opt/atheme/help/nickserv/allowmail
}

if [ "${1:-}" = module ]; then
	[ -d "$NICKSERV" ] || { echo "no Atheme tree in $TREE; run without 'module' first" >&2; exit 1; }
	module
	exit 0
fi

if [ -x /opt/atheme/bin/atheme-services ] &&
   /opt/atheme/bin/atheme-services -v 2>/dev/null | grep -q "atheme $VERSION"; then
	echo "Atheme $VERSION already installed; rebuilding the module only"
	module
	exit 0
fi

tmp=$(mktemp -d)
curl -fsSL -o "$tmp/atheme.tar.xz" \
	"https://github.com/atheme/atheme/releases/download/v$VERSION/atheme-services-v$VERSION.tar.xz"
tar -C "$tmp" -xJf "$tmp/atheme.tar.xz"
rm -rf "$TREE"
mv "$tmp/atheme-services-v$VERSION" "$TREE"
rm -rf "$tmp"

# Add allowmail.c to the nickserv module list, next to badmail.c.
sed -i 's/^\tbadmail\.c\t\\$/\tbadmail.c\t\\\n\tallowmail.c\t\\/' "$NICKSERV/Makefile"
grep -q 'allowmail\.c' "$NICKSERV/Makefile" || { echo "could not patch $NICKSERV/Makefile" >&2; exit 1; }
cp "$SRC/atheme/allowmail.c" "$NICKSERV/allowmail.c"
cp "$SRC/atheme/allowmail.help" "$TREE/help/default/nickserv/allowmail"

cd "$TREE"
./configure --prefix=/opt/atheme --disable-nls > /dev/null
make -j"$(nproc)" > /dev/null 2> "$TREE/build.log" || { tail -20 "$TREE/build.log" >&2; exit 1; }
make install > /dev/null
echo "Atheme $VERSION installed in /opt/atheme"
