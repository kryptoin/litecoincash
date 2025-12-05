#!/bin/bash
# Run all sanity tests
# These tests do NOT require chain startup or RPC

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================"
echo "Running Litecoin Cash Sanity Tests"
echo "========================================"
echo ""

FAILED=0

# Test 1: Binary checks
echo ">>> Running sanity_binary_check.py..."
python3 "$SCRIPT_DIR/sanity_binary_check.py"
if [ $? -ne 0 ]; then
    FAILED=1
fi
echo ""

# Test 2: Config checks
echo ">>> Running sanity_config_check.py..."
python3 "$SCRIPT_DIR/sanity_config_check.py"
if [ $? -ne 0 ]; then
    FAILED=1
fi
echo ""

# Test 3: Datadir checks
echo ">>> Running sanity_datadir_check.py..."
python3 "$SCRIPT_DIR/sanity_datadir_check.py"
if [ $? -ne 0 ]; then
    FAILED=1
fi
echo ""

echo "========================================"
if [ $FAILED -eq 0 ]; then
    echo "ALL SANITY TESTS PASSED"
    exit 0
else
    echo "SOME SANITY TESTS FAILED"
    exit 1
fi
