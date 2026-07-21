#!/bin/sh

set -eu

client=${1:-./build/modbusc}
server=${2:-./build/modbuss}

fail()
{
    echo "cli tests: FAIL: $*" >&2
    exit 1
}

expect_success()
{
    expected=$1
    shift
    output=$("$@" 2>&1) || fail "expected success from '$*', got: $output"
    case $output in
        *"$expected"*) ;;
        *) fail "'$*' output did not contain '$expected': $output" ;;
    esac
}

expect_failure()
{
    expected=$1
    shift
    set +e
    output=$("$@" 2>&1)
    status=$?
    set -e
    [ "$status" -ne 0 ] || fail "expected failure from '$*'"
    case $output in
        *"$expected"*) ;;
        *) fail "'$*' output did not contain '$expected': $output" ;;
    esac
}

expect_success "Usage:" "$client" --help
expect_success "0.2.1" "$client" --version
expect_success "--ip" "$client" tcp --help
expect_success "--data-bits" "$client" rtu --help
expect_success "Usage:" "$server" --help
expect_success "0.2.1" "$server" --version
expect_success "--ip" "$server" tcp --help
expect_success "--data-bits" "$server" rtu --help

expect_failure "slave address" "$client" tcp -a 300 -r 0 -f 3
expect_failure "increasing range" "$client" tcp -a 2.1 -r 0 -f 3
expect_failure "register range" "$client" tcp -a 1 -r 0 -1 -f 3
expect_failure "timeout must be" "$client" tcp -a 1 -r 0 -f 3 -o 0
expect_failure "register read count" "$client" tcp -a 1 -r 0 -f 3 -c 126
expect_failure "exactly one" "$client" tcp -a 1 -r 0 -f 6
expect_failure "coil values" "$client" tcp -a 1 -r 0 -f 5 -w 2
expect_failure "TCP port" "$client" tcp -a 1 -r 0 -f 3 -p 70000
expect_failure "TCP port" "$server" tcp -p 70000
expect_failure "mapping sizes" "$server" tcp --co 0

echo "cli tests: PASS"
