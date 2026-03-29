#!/bin/bash

DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DIR"
PORT=9879
PASS=0
FAIL=0

check() {
    local desc="$1"
    local expected="$2"
    local actual="$3"
    if echo "$actual" | grep -q "$expected"; then
        echo "  PASS: $desc"
        PASS=$((PASS+1))
    else
        echo "  FAIL: $desc"
        echo "    Expected: $expected"
        echo "    Got: $actual"
        FAIL=$((FAIL+1))
    fi
}

cleanup() {
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null
    ./bank_deinit 2>/dev/null || true
}

echo "=== Concurrent / Network Tests ==="

# cleanup old state
./bank_deinit 2>/dev/null || true
# kill anything on our port
fuser -k $PORT/tcp 2>/dev/null || true
sleep 1

# start server in background (server creates its own bank)
./server 3 $PORT &
SERVER_PID=$!
sleep 2

echo "--- Network client basic ---"
OUT=$(echo -e "balance 0\ndeposit 100\nbalance 0\ntransfer 0 1 40\nbalance 0\nbalance 1" | ./client_net 127.0.0.1 $PORT 2>/dev/null)
check "net initial balance" "Balance of account 0: 0" "$OUT"
check "net deposit" "Deposited 100 to all" "$OUT"
check "net transfer" "Transferred 40" "$OUT"
check "net balance after transfer" "Balance of account 0: 60" "$OUT"
check "net receiver balance" "Balance of account 1: 140" "$OUT"

echo "--- Concurrent transfers ---"
# Run 3 clients in parallel, each transferring 10 from acc 0 to acc 1
PIDS=""
for i in 1 2 3; do
    echo "transfer 0 1 10" | ./client_net 127.0.0.1 $PORT 2>/dev/null &
    PIDS="$PIDS $!"
done
for pid in $PIDS; do
    wait $pid
done

OUT=$(echo "balance 0" | ./client_net 127.0.0.1 $PORT 2>/dev/null)
check "balance after concurrent transfers" "Balance of account 0: 30" "$OUT"

OUT=$(echo "balance 1" | ./client_net 127.0.0.1 $PORT 2>/dev/null)
check "receiver after concurrent transfers" "Balance of account 1: 170" "$OUT"

echo "--- Shutdown ---"
echo "shutdown" | ./client_net 127.0.0.1 $PORT 2>/dev/null || true
sleep 3

# check server is gone
if kill -0 $SERVER_PID 2>/dev/null; then
    kill $SERVER_PID 2>/dev/null
    echo "  FAIL: server did not shut down"
    FAIL=$((FAIL+1))
else
    echo "  PASS: server shut down cleanly"
    PASS=$((PASS+1))
fi

wait $SERVER_PID 2>/dev/null
./bank_deinit 2>/dev/null || true

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ $FAIL -eq 0 ] && exit 0 || exit 1
