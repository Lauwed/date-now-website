#!/bin/sh
# Smoke test of the API under a memory checker, run in the dev image (see
# .github/workflows/api-sanitizers.yml).
#
#   smoke.sh asan       build with AddressSanitizer + UBSan, then run
#   smoke.sh valgrind   run the normal build under Valgrind
#
# Needs a Postgres database at DATABASE_URL. Exits with a non-zero code when a
# request gives a wrong status, or when the checker finds an error or a leak.
set -eu

mode=${1:?"usage: smoke.sh asan|valgrind"}

: "${DATABASE_URL:?DATABASE_URL is required}"
export DB_ENCRYPTION_KEY="${DB_ENCRYPTION_KEY:-MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MDE=}"
export JWT_SECRET="${JWT_SECRET:-ci-smoke-test-jwt-secret-32-chars!}"
export CORS_ORIGIN="${CORS_ORIGIN:-*}"
# No DNS lookup in CI: the blocklist and the format are still checked
export EMAIL_DNS_CHECK="${EMAIL_DNS_CHECK:-0}"

apk add --no-cache curl postgresql-client >/dev/null

case "$mode" in
  asan)
    apk add --no-cache clang compiler-rt >/dev/null
    rm -f bin/serv_api obj/*.o obj/*/*.o
    make CC=clang \
      EXTRA_CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
      EXTRA_LDFLAGS="-fsanitize=address,undefined"
    export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1
    export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
    start="./bin/serv_api"
    ;;
  valgrind)
    apk add --no-cache valgrind >/dev/null
    start="valgrind --leak-check=full --show-leak-kinds=definite,indirect --error-exitcode=1 --log-file=valgrind.log ./bin/serv_api"
    ;;
  *)
    echo "unknown mode: $mode" >&2
    exit 2
    ;;
esac

for f in migrations/postgres/*.sql; do
  psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -q -f "$f" 2>&1 | grep -v NOTICE || true
done

# shellcheck disable=SC2086
$start &
server=$!

# Valgrind starts slowly: wait up to 60 s for the server
for _ in $(seq 1 120); do
  curl -s http://localhost:8000/ >/dev/null 2>&1 && break
  sleep 0.5
done

status() { curl -s -o /dev/null -w "%{http_code}" "$@"; }
fail=0
expect() { # expect <label> <wanted status> <got status>
  echo "$1 -> HTTP $3 (wanted $2)"
  [ "$3" = "$2" ] || fail=1
}

expect "GET /api/issue" 200 "$(status http://localhost:8000/api/issue)"
expect "GET /api/tag" 200 "$(status http://localhost:8000/api/tag)"
expect "GET /api/sponsor" 200 "$(status http://localhost:8000/api/sponsor)"

# The blocklist is not public without BLOCKED_DOMAIN_LIST_PUBLIC
expect "GET /api/blocked-domain" 401 "$(status http://localhost:8000/api/blocked-domain)"

# A domain of the seed is refused
expect "POST /api/auth/subscribe (blocked domain)" 400 "$(status -X POST \
  http://localhost:8000/api/auth/subscribe -H 'Content-Type: application/json' \
  -d '{"email":"smoke@mailinator.com"}')"

# An accepted domain goes on to the mail, which is not configured: 400 as well
code=$(status -X POST http://localhost:8000/api/auth/subscribe \
  -H 'Content-Type: application/json' -d '{"email":"smoke@test.example"}')
echo "POST /api/auth/subscribe (accepted domain) -> HTTP $code (not a 5xx)"
[ "$code" -lt 500 ] || fail=1

# SIGTERM -> handle_shutdown -> exit(0): the checker reports at exit, and
# changes the exit code when it found something
kill "$server"
wait "$server" || fail=1

[ -f valgrind.log ] && cat valgrind.log
exit "$fail"
