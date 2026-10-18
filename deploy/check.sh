#!/bin/sh
# Checks that need no secrets and no server. Run by `make check` and CI.
set -eu
cd "$(dirname "$0")/.."

sh -n deploy/*.sh deploy/render

python3 -m json.tool kiwi/config.json > /dev/null

# channels.tsv: channel <tab> yes|no <tab> description
awk -F'\t' '
	NF != 3 || (NR > 1 && ($1 !~ /^[#&][^ ,]+$/ || ($2 != "yes" && $2 != "no"))) {
		printf "channels.tsv:%d: want: #channel<TAB>yes|no<TAB>description\n", NR
		bad = 1
	}
	END { exit bad }
' channels.tsv

# the landing page lists every channel in channels.tsv
awk -F'\t' 'NR > 1 { print $1 }' channels.tsv |
while read -r channel; do
	grep -qF "#${channel#\#}</a>" www/index.html ||
		{ echo "www/index.html: $channel is in channels.tsv but not on the page" >&2; exit 1; }
done

# every local href/src in the pages points at a file that exists
for page in www/index.html www/cs/index.html; do
	dir=$(dirname "$page")
	grep -o '\(href\|src\)="[^"]*"' "$page" |
	sed 's/^[a-z]*="//; s/"$//; s/[?#].*//' |
	grep -v ':' |
	while read -r path; do
		[ -n "$path" ] || continue
		[ -e "$dir/$path" ] || { echo "$page: missing $path" >&2; exit 1; }
	done
done

echo "check: ok"
