#!/bin/bash
#
# run.sh - exercise statement_mode = legacy and prepared against the same
# MySQL/MariaDB test database, and verify that the two outputs match
# (modulo identical warnings on identical inputs).
#
# The script (re)creates the test schema, generates paired .cf files for
# each test case (one per mode), runs `postmap -q` against both, and
# diffs stdout + stderr.
#
# Usage:
#   POSTMAP=./bin/postmap \
#   MYSQL_USER=postfix MYSQL_PASSWORD=secret MYSQL_HOST=127.0.0.1 \
#   MYSQL_PORT=3306 MYSQL_DATABASE=postfix_test \
#   MAIL_CONFIG=/path/to/temp-conf \
#       bash ./postfix-env.sh tests/mysql_prepared/run.sh
#
# Environment:
#   POSTMAP          path to postmap (default: postmap from PATH)
#   MYSQL            path to mysql CLI (default: mysql from PATH)
#   MYSQL_HOST       mysql host (default: 127.0.0.1)
#   MYSQL_PORT       mysql port (default: 3306)
#   MYSQL_USER       role used to connect (default: $USER)
#   MYSQL_PASSWORD   password (default: empty)
#   MYSQL_DATABASE   test database name (default: postfix_test)

set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
CONF="$DIR/conf"
OUT="$DIR/out"
POSTMAP="${POSTMAP:-postmap}"
MYSQL_BIN="${MYSQL:-mysql}"
MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-$USER}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-}"
MYSQL_DATABASE="${MYSQL_DATABASE:-postfix_test}"

mkdir -p "$CONF" "$OUT"
rm -f "$OUT"/*

mysql_run() {
    if [ -n "$MYSQL_PASSWORD" ]; then
        MYSQL_PWD="$MYSQL_PASSWORD" "$MYSQL_BIN" \
            -h "$MYSQL_HOST" -P "$MYSQL_PORT" -u "$MYSQL_USER" \
            "$MYSQL_DATABASE" "$@"
    else
        "$MYSQL_BIN" -h "$MYSQL_HOST" -P "$MYSQL_PORT" -u "$MYSQL_USER" \
            "$MYSQL_DATABASE" "$@"
    fi
}

# ----------------------------------------------------------------------
# Build the test schema. Three tables exercise the interesting cases.
# ----------------------------------------------------------------------
mysql_run <<'SQL'
DROP TABLE IF EXISTS aliases;
DROP TABLE IF EXISTS virtual;
DROP TABLE IF EXISTS bydomainpart;

CREATE TABLE aliases (name VARCHAR(64), target VARCHAR(255));
INSERT INTO aliases VALUES
    ('postmaster', 'admin@example.com'),
    ('webmaster',  NULL),
    ('emptyrow',   ''),
    ('multi',      'a@example.com'),
    ('multi',      'b@example.com');

CREATE TABLE virtual (local VARCHAR(64), domain VARCHAR(255), target VARCHAR(255));
INSERT INTO virtual VALUES
    ('alice', 'example.com', 'alice-real@host.example.com'),
    ('bob',   'example.com', 'bob-real@host.example.com');

CREATE TABLE bydomainpart (label1 VARCHAR(64), label2 VARCHAR(64), target VARCHAR(255));
INSERT INTO bydomainpart VALUES
    ('com', 'example', 'example.com served here'),
    ('org', 'example', 'example.org served here');
SQL

# ----------------------------------------------------------------------
# Generate paired .cf files: identical except for statement_mode.
# ----------------------------------------------------------------------
gen_pair() {
    local stem="$1" body="$2" mode auth
    auth=""
    if [ -n "$MYSQL_PASSWORD" ]; then
        auth="password = $MYSQL_PASSWORD"
    fi
    for mode in legacy prepared; do
        {
            echo "hosts = inet:$MYSQL_HOST:$MYSQL_PORT"
            echo "user = $MYSQL_USER"
            [ -n "$auth" ] && echo "$auth"
            echo "dbname = $MYSQL_DATABASE"
            echo "$body"
            echo "statement_mode = $mode"
        } > "$CONF/${stem}-${mode}.cf"
    done
}

gen_pair aliases  "query = SELECT target FROM aliases WHERE name = '%s'"
gen_pair virtual  "query = SELECT target FROM virtual WHERE local = '%u' AND domain = '%d'"
gen_pair bydomain "query = SELECT target FROM bydomainpart WHERE label1 = '%1' AND label2 = '%2'"
gen_pair vfilter  "query = SELECT target FROM virtual WHERE local = '%u' AND domain = '%d'
domain = example.com"
gen_pair limit    "query = SELECT target FROM aliases WHERE name = '%s'
expansion_limit = 1"

# ----------------------------------------------------------------------
# Run a test case in both modes and diff the results.
# ----------------------------------------------------------------------
PASS=0
FAIL=0

normalize() {
    sed -E 's#-(legacy|prepared)\.cf#.cf#g; s#_(legacy|prepared)##g'
}

filter_warnings() {
    grep -E "(warning|fatal|panic):" || true
}

run_pair() {
    local label="$1" stem="$2" key="$3" mode rc

    for mode in legacy prepared; do
        rc=0
        "$POSTMAP" -q "$key" "mysql:$CONF/${stem}-${mode}.cf" \
            > "$OUT/${label}-${mode}.stdout.raw" \
            2> "$OUT/${label}-${mode}.stderr.raw" \
            || rc=$?
        echo "exit=$rc" > "$OUT/${label}-${mode}.exit"
        normalize < "$OUT/${label}-${mode}.stdout.raw"  > "$OUT/${label}-${mode}.stdout"
        filter_warnings < "$OUT/${label}-${mode}.stderr.raw" \
            | normalize > "$OUT/${label}-${mode}.stderr"
    done

    if diff -q "$OUT/${label}-legacy.stdout" "$OUT/${label}-prepared.stdout" >/dev/null \
        && diff -q "$OUT/${label}-legacy.stderr" "$OUT/${label}-prepared.stderr" >/dev/null
    then
        printf "PASS  %-22s key=%-22s -> %s\n" "$label" "$key" \
            "$(tr '\n' ' ' < "$OUT/${label}-legacy.stdout" | sed 's/ *$//')"
        PASS=$((PASS+1))
    else
        printf "FAIL  %-22s key=%-22s\n" "$label" "$key"
        echo "  ---- stdout diff (legacy vs prepared) ----"
        diff "$OUT/${label}-legacy.stdout" "$OUT/${label}-prepared.stdout" \
            | sed 's/^/  /' || true
        echo "  ---- stderr diff (legacy vs prepared) ----"
        diff "$OUT/${label}-legacy.stderr" "$OUT/${label}-prepared.stderr" \
            | sed 's/^/  /' || true
        FAIL=$((FAIL+1))
    fi
}

# ----------------------------------------------------------------------
# Test cases. See tests/sqlite_prepared/run.sh for the per-case
# rationale. The null-value case here is the regression covered by the
# is_null indicator added in plmysql_query_prepared() -- without it,
# prepared mode would have warned and returned an empty string, while
# legacy mode silently returns no result.
# ----------------------------------------------------------------------
run_pair simple-found       aliases   postmaster
run_pair simple-not-found   aliases   nonexistent
run_pair null-value         aliases   webmaster
run_pair empty-value        aliases   emptyrow
run_pair multi-row          aliases   multi
run_pair virtual-found      virtual   alice@example.com
run_pair virtual-bare-key   virtual   alice
run_pair domain-filter-skip vfilter   bob@other.com
run_pair empty-key          virtual   ''
run_pair expansion-limit    limit     multi
run_pair domain-parts       bydomain  user@example.com

echo
echo "$PASS passed, $FAIL failed."
echo "Outputs in: $OUT"
[ "$FAIL" -eq 0 ]
