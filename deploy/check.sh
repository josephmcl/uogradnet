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
