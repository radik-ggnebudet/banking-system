#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DIR"
PORT=9878
ERRORS=0

echo "=== Valgrind Memcheck Tests ==="

# cleanup
./bank_deinit 2>/dev/null || true

# Test init + local client under memcheck
echo "--- memcheck: bank_init ---"
valgrind --tool=memcheck --leak-check=full --error-exitcode=1 \
    ./bank_init 3 2>&1 | tail -5
echo ""

echo "--- memcheck: client_local ---"
echo -e "balance 0\ndeposit 50\ntransfer 0 1 20\nfreeze 2\nunfreeze 2" | \
    valgrind --tool=memcheck --leak-check=full --error-exitcode=1 \
    ./client_local 2>&1 | tail -10
echo ""

echo "--- memcheck: bank_deinit ---"
valgrind --tool=memcheck --leak-check=full --error-exitcode=1 \
    ./bank_deinit 2>&1 | tail -5
echo ""

echo "=== Valgrind Helgrind Tests ==="

# Test server under helgrind
echo "--- helgrind: server + clients ---"
./bank_deinit 2>/dev/null || true

valgrind --tool=helgrind --error-exitcode=1 \
    ./server 3 $PORT &
VPID=$!
sleep 2

echo -e "deposit 100\ntransfer 0 1 50\nbalance 0\nbalance 1" | \
    ./client_net 127.0.0.1 $PORT 2>/dev/null

# concurrent clients
for i in 1 2 3; do
    echo "transfer 0 1 5" | ./client_net 127.0.0.1 $PORT 2>/dev/null &
done
wait

echo "shutdown" | ./client_net 127.0.0.1 $PORT 2>/dev/null || true
sleep 2

# check if valgrind process exited
if kill -0 $VPID 2>/dev/null; then
    kill $VPID 2>/dev/null
fi
wait $VPID 2>/dev/null || true

./bank_deinit 2>/dev/null || true

echo ""
echo "=== Valgrind tests completed ==="
