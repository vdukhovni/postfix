#!/bin/bash
#
# run.sh - exercise statement_mode = legacy, parameterized, and prepared
# against the same PostgreSQL test database, and verify that the three
# outputs match (modulo identical warnings on identical inputs).
#
# The script (re)creates the test schema, generates triplet .cf files
# for each test case (one per mode), runs `postmap -q` against all
# three, and diffs stdout + stderr. Anything that differs in any pair
# is a behavioural regression in one of the three paths.
#
# Usage:
#   POSTMAP=./bin/postmap \
#   PGUSER=viktor PGDATABASE=postfix_test PGHOST=127.0.0.1 PGPORT=54320 \
#   MAIL_CONFIG=/path/to/temp-conf \
#       bash ./postfix-env.sh tests/pgsql_prepared/run.sh
#
# Environment:
#   POSTMAP        path to postmap (default: postmap from PATH)
#   PSQL           path to psql CLI (default: psql from PATH)
#   PGHOST         postgres host (default: 127.0.0.1)
#   PGPORT         postgres port (default: 5432)
#   PGUSER         role used to connect (default: $USER)
#   PGDATABASE     test database name (default: postfix_test)

set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
CONF="$DIR/conf"
OUT="$DIR/out"
POSTMAP="${POSTMAP:-postmap}"
PSQL="${PSQL:-psql}"
PGHOST="${PGHOST:-127.0.0.1}"
PGPORT="${PGPORT:-5432}"
PGUSER="${PGUSER:-$USER}"
PGDATABASE="${PGDATABASE:-postfix_test}"

mkdir -p "$CONF" "$OUT"
rm -f "$OUT"/*

export PGHOST PGPORT PGUSER PGDATABASE

# ----------------------------------------------------------------------
# Build the test schema. Three tables exercise the interesting cases:
#
#   aliases       single-column key, single-column value, with rows that
#                 have NULL, empty-string, and multi-row values to
#                 exercise the result accumulator and the null-vs-empty
#                 distinction.
#   virtual       lookup by user@domain, exercises %u and %d.
#   bydomainpart  lookup by domain labels, exercises %1 and %2.
#
# DROPped first so the script is idempotent.
# ----------------------------------------------------------------------
"$PSQL" -v ON_ERROR_STOP=1 -X -q <<'SQL'
DROP TABLE IF EXISTS aliases, virtual, bydomainpart;

CREATE TABLE aliases (name TEXT, target TEXT);
INSERT INTO aliases VALUES
    ('postmaster', 'admin@example.com'),
    ('webmaster',  NULL),
    ('emptyrow',   ''),
    ('multi',      'a@example.com'),
    ('multi',      'b@example.com');

CREATE TABLE virtual (local TEXT, domain TEXT, target TEXT);
INSERT INTO virtual VALUES
    ('alice', 'example.com', 'alice-real@host.example.com'),
    ('bob',   'example.com', 'bob-real@host.example.com');

CREATE TABLE bydomainpart (label1 TEXT, label2 TEXT, target TEXT);
INSERT INTO bydomainpart VALUES
    ('com', 'example', 'example.com served here'),
    ('org', 'example', 'example.org served here');
SQL

# ----------------------------------------------------------------------
# Generate triplet .cf files: identical except for statement_mode.
# ----------------------------------------------------------------------
gen_triplet() {
    local stem="$1" body="$2" mode
    for mode in legacy parameterized prepared; do
        {
            echo "hosts = inet:$PGHOST:$PGPORT"
            echo "user = $PGUSER"
            echo "dbname = $PGDATABASE"
            echo "$body"
            echo "statement_mode = $mode"
        } > "$CONF/${stem}-${mode}.cf"
    done
}

gen_triplet aliases  "query = SELECT target FROM aliases WHERE name = '%s'"
gen_triplet virtual  "query = SELECT target FROM virtual WHERE local = '%u' AND domain = '%d'"
gen_triplet bydomain "query = SELECT target FROM bydomainpart WHERE label1 = '%1' AND label2 = '%2'"
gen_triplet vfilter  "query = SELECT target FROM virtual WHERE local = '%u' AND domain = '%d'
domain = example.com"
gen_triplet limit    "query = SELECT target FROM aliases WHERE name = '%s'
expansion_limit = 1"

# ----------------------------------------------------------------------
# Run a test case in all three modes and diff the results pairwise.
# ----------------------------------------------------------------------
PASS=0
FAIL=0

normalize() {
    # Strip the per-mode suffix from .cf paths and from the *_legacy /
    # *_prepared function-name suffix used in msg_warn() output, so
    # that semantically-equal warnings compare equal.
    sed -E 's#-(legacy|parameterized|prepared)\.cf#.cf#g; s#_(legacy|prepared)##g'
}

filter_warnings() {
    grep -E "(warning|fatal|panic):" || true
}

run_triplet() {
    local label="$1" stem="$2" key="$3" mode rc base

    for mode in legacy parameterized prepared; do
        rc=0
        "$POSTMAP" -q "$key" "pgsql:$CONF/${stem}-${mode}.cf" \
            > "$OUT/${label}-${mode}.stdout.raw" \
            2> "$OUT/${label}-${mode}.stderr.raw" \
            || rc=$?
        echo "exit=$rc" > "$OUT/${label}-${mode}.exit"
        normalize < "$OUT/${label}-${mode}.stdout.raw"  > "$OUT/${label}-${mode}.stdout"
        filter_warnings < "$OUT/${label}-${mode}.stderr.raw" \
            | normalize > "$OUT/${label}-${mode}.stderr"
    done

    base=legacy
    local ok=1
    for mode in parameterized prepared; do
        if ! diff -q "$OUT/${label}-${base}.stdout" "$OUT/${label}-${mode}.stdout" >/dev/null \
            || ! diff -q "$OUT/${label}-${base}.stderr" "$OUT/${label}-${mode}.stderr" >/dev/null
        then
            ok=0
            printf "FAIL  %-22s key=%-22s (%s vs %s)\n" "$label" "$key" "$base" "$mode"
            echo "  ---- stdout diff ----"
            diff "$OUT/${label}-${base}.stdout" "$OUT/${label}-${mode}.stdout" \
                | sed 's/^/  /' || true
            echo "  ---- stderr diff ----"
            diff "$OUT/${label}-${base}.stderr" "$OUT/${label}-${mode}.stderr" \
                | sed 's/^/  /' || true
        fi
    done
    if [ "$ok" -eq 1 ]; then
        printf "PASS  %-22s key=%-22s -> %s\n" "$label" "$key" \
            "$(tr '\n' ' ' < "$OUT/${label}-${base}.stdout" | sed 's/ *$//')"
        PASS=$((PASS+1))
    else
        FAIL=$((FAIL+1))
    fi
}

# ----------------------------------------------------------------------
# Test cases (same as sqlite_prepared, plus a third leg for parameterized
# mode). See tests/sqlite_prepared/run.sh for the per-case rationale.
# ----------------------------------------------------------------------
run_triplet simple-found       aliases   postmaster
run_triplet simple-not-found   aliases   nonexistent
run_triplet null-value         aliases   webmaster
run_triplet empty-value        aliases   emptyrow
run_triplet multi-row          aliases   multi
run_triplet virtual-found      virtual   alice@example.com
run_triplet virtual-bare-key   virtual   alice
run_triplet domain-filter-skip vfilter   bob@other.com
run_triplet empty-key          virtual   ''
run_triplet expansion-limit    limit     multi
run_triplet domain-parts       bydomain  user@example.com

echo
echo "$PASS passed, $FAIL failed."
echo "Outputs in: $OUT"
[ "$FAIL" -eq 0 ]
