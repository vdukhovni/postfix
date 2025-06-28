#!/bin/sh
set -ex
! ${VALGRIND} ./dict_open 'debug:' read </dev/null || exit 1
! ${VALGRIND} ./dict_open 'debug:missing_colon_and_name' read </dev/null || exit 1
${VALGRIND} ./dict_open 'debug:static:{space in name}' read <<EOF
get k
EOF
${VALGRIND} ./dict_open 'debug:debug:static:value' read <<EOF
get k
EOF
${VALGRIND} ./dict_open 'debug:internal:name' write <<EOF
get k
put k=v
get k
first
next
del k
get k
del k
first
EOF
${VALGRIND} ./dict_open 'debug:fail:{oh no}' read <<EOF
get k
first
EOF
