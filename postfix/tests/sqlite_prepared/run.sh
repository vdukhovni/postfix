#!/bin/bash
#
# run.sh - exercise statement_mode = legacy and statement_mode = prepared
# against the same SQLite test database, and verify that the two outputs
# match (modulo identical warnings on identical inputs).
#
# The script builds a small test database, generates paired .cf files for
# each test case (one for each mode), runs `postmap -q` against both, and
# diffs stdout + stderr. Anything that differs is a behavioural regression
# in one of the two paths.
#
# Usage:
#   POSTMAP=./bin/postmap \
#   MAIL_CONFIG=/path/to/temp-conf \
#       bash ./postfix-env.sh tests/sqlite_prepared/run.sh
#
# Environment:
#   POSTMAP   path to postmap (default: postmap, expects PATH)
#   SQLITE3   path to sqlite3 CLI (default: sqlite3, expects PATH)

set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
DB="$DIR/test.db"
CONF="$DIR/conf"
OUT="$DIR/out"
POSTMAP="${POSTMAP:-postmap}"
SQLITE3="${SQLITE3:-sqlite3}"

mkdir -p "$CONF" "$OUT"
rm -f "$DB" "$OUT"/*

# ----------------------------------------------------------------------
# Build the test database. Three tables exercise the interesting cases:
#
#   aliases       - single-column key, single-column value, with rows that
#                   have NULL, empty-string, and multi-row values to
#                   exercise the result accumulator and the null-vs-empty
#                   distinction.
#   virtual       - lookup by user@domain, exercises %u and %d.
#   bydomainpart  - lookup by domain labels, exercises %1 and %2.
# ----------------------------------------------------------------------
"$SQLITE3" "$DB" <<'SQL'
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
# Generate paired .cf files: identical except for statement_mode.
# ----------------------------------------------------------------------
gen_pair() {
    local stem="$1" body="$2" mode
    for mode in legacy prepared; do
        {
            echo "dbpath = $DB"
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
#
# Stderr is filtered to warnings/fatals/panics so that non-warning chatter
# (e.g. "open database" trace messages from a verbose-enabled build) does
# not cause spurious mismatches. The path prefix in warnings is stripped
# (otherwise legacy and prepared cf paths would always differ).
# ----------------------------------------------------------------------
PASS=0
FAIL=0

normalize() {
    # Strip the per-mode suffix from .cf paths and from the *_legacy /
    # *_prepared function-name suffix used in msg_warn() output, so
    # that semantically-equal warnings compare equal.
    sed -E 's#-(legacy|prepared)\.cf#.cf#g; s#_(legacy|prepared)##g'
}

filter_warnings() {
    grep -E "(warning|fatal|panic):" || true
}

run_pair() {
    local label="$1" stem="$2" key="$3" mode rc

    for mode in legacy prepared; do
        rc=0
        "$POSTMAP" -q "$key" "sqlite:$CONF/${stem}-${mode}.cf" \
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
# Test cases.
#
#   simple-found       row exists, single value
#   simple-not-found   no row matches; no output, no warning
#   null-value         row exists with NULL value: silently skipped (both
#                      modes go through db_common_expand which short-
#                      circuits NULL inputs)
#   empty-value        row exists with "" value: produces an "empty lookup
#                      result" warning in both modes, no output
#   multi-row          two rows match: result is comma-joined
#   virtual-found      qualified address with %u and %d
#   virtual-bare-key   unqualified key: %d has nothing to expand to, so
#                      query is suppressed and returns no result
#   domain-filter-skip key falls outside the domain= filter: silent skip
#   empty-key          empty lookup key: ONE warning expected in each mode
#                      (a regression in prepared mode would emit one per
#                      parameter slot - this case covers two slots)
#   expansion-limit    multi-row result with expansion_limit = 1: warning
#                      and DICT_ERR_RETRY in both modes
#   domain-parts       %1 and %2 substitutions for domain labels
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
