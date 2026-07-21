#!/bin/sh

set -eu

client=${1:-./build/modbusc}
server=${2:-./build/modbuss}
port=${MODBUS_TEST_PORT:-15020}
server_log=$(mktemp)
server_pid=

cleanup()
{
    if [ -n "$server_pid" ]; then
        kill -TERM "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
    rm -f "$server_log"
}
trap cleanup EXIT INT TERM

"$server" tcp --ip 127.0.0.1 --port "$port" >"$server_log" 2>&1 &
server_pid=$!

ready=0
attempt=0
while [ "$attempt" -lt 30 ]; do
    if "$client" tcp --ip 127.0.0.1 --port "$port" --addr 1 --reg 0 --func 3 --count 1 \
        >/dev/null 2>&1; then
        ready=1
        break
    fi
    if ! kill -0 "$server_pid" 2>/dev/null; then
        echo "tcp loopback: server exited during startup" >&2
        sed -n '1,120p' "$server_log" >&2
        exit 1
    fi
    attempt=$((attempt + 1))
    sleep 0.1
done
[ "$ready" -eq 1 ] || {
    echo "tcp loopback: server did not become ready" >&2
    sed -n '1,120p' "$server_log" >&2
    exit 1
}

run_client()
{
    "$client" tcp --ip 127.0.0.1 --port "$port" --addr 1 --reg 0 "$@"
}

run_client --func 2 --count 3 | grep -q "0x00 0x00 0x00"
run_client --func 4 --count 2 | grep -q "0x0000 0x0000"

run_client --func 5 --write 1 | grep -q "SUCCESS"
run_client --func 1 --count 1 | grep -q "0x01"

run_client --func 6 --write 4660 | grep -q "SUCCESS"
run_client --func 3 --count 1 | grep -q "0x1234"

run_client --func 15 --write 1 --write 0 --write 1 | grep -q "SUCCESS"
run_client --func 1 --count 3 | grep -q "0x01 0x00 0x01"

run_client --func 16 --write 1 --write 2 --write 65535 | grep -q "SUCCESS"
run_client --func 3 --count 3 | grep -q "0x0001 0x0002 0xffff"

kill -TERM "$server_pid"
wait "$server_pid"
server_pid=

echo "tcp loopback tests: PASS"
