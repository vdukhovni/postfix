#!/bin/sh

set -e

rm -f foo.lmdb

./dict_cache <<EOF
cache lmdb:foo
update x ${1-2000}
run 
update y ${1-2000}
purge x
run 
purge y
run 
EOF

../../bin/postmap -s lmdb:foo | diff /dev/null -
rm -f foo.lmdb
