#!/bin/bash

# End-to-end tests for the string processing pipeline

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PIPELINE="./build/bin/pipeline"
PLUGIN_DIR="./build/lib/plugins"

echo -e "${YELLOW}Running End-to-End Tests${NC}"
echo "=============================="

# Test counter
TESTS=0
PASSED=0
FAILED=0

run_test() {
    local name="$1"
    local input="$2"
    local plugins="$3"
    local expected="$4"
    
    TESTS=$((TESTS + 1))
    
    echo -n "Test $TESTS: $name... "
    
    # Run pipeline
    actual=$(echo -e "$input" | $PIPELINE $plugins 2>/dev/null | grep -v "^Loaded plugin:" | tr '\n' '|' | sed 's/|$//')
    expected_formatted=$(echo -e "$expected" | tr '\n' '|' | sed 's/|$//')
    
    if [ "$actual" = "$expected_formatted" ]; then
        echo -e "${GREEN}PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}FAILED${NC}"
        echo "  Expected: $expected_formatted"
        echo "  Got:      $actual"
        FAILED=$((FAILED + 1))
    fi
}

# Test 1: Single plugin - uppercase
run_test "Single uppercase plugin" \
    "hello\nworld\n<END>" \
    "$PLUGIN_DIR/upper.so" \
    "HELLO\nWORLD"

# Test 2: Single plugin - lowercase  
run_test "Single lowercase plugin" \
    "HELLO\nWORLD\n<END>" \
    "$PLUGIN_DIR/lower.so" \
    "hello\nworld"

# Test 3: Single plugin - reverse
run_test "Single reverse plugin" \
    "hello\nworld\n<END>" \
    "$PLUGIN_DIR/reverse.so" \
    "olleh\ndlrow"

# Test 4: Two plugins - upper then reverse
run_test "Upper then reverse" \
    "hello\nworld\n<END>" \
    "$PLUGIN_DIR/upper.so $PLUGIN_DIR/reverse.so" \
    "OLLEH\nDLROW"

# Test 5: Three plugins - trim, upper, prefix
run_test "Trim, upper, prefix" \
    "  hello  \n  world  \n<END>" \
    "$PLUGIN_DIR/trim.so $PLUGIN_DIR/upper.so $PLUGIN_DIR/prefix.so" \
    "PREFIX:HELLO\nPREFIX:WORLD"

# Test 6: All transformation plugins
run_test "All six plugins" \
    "  hello  \n<END>" \
    "$PLUGIN_DIR/trim.so $PLUGIN_DIR/upper.so $PLUGIN_DIR/reverse.so $PLUGIN_DIR/prefix.so $PLUGIN_DIR/suffix.so $PLUGIN_DIR/lower.so" \
    "prefix:olleh:suffix"

# Test 7: Empty input
run_test "Empty input with END marker" \
    "<END>" \
    "$PLUGIN_DIR/upper.so" \
    ""

# Test 8: Single line
run_test "Single line input" \
    "test\n<END>" \
    "$PLUGIN_DIR/upper.so $PLUGIN_DIR/reverse.so" \
    "TSET"

# Test 9: Special characters
run_test "Special characters" \
    "hello@world.com\n123-456\n<END>" \
    "$PLUGIN_DIR/upper.so" \
    "HELLO@WORLD.COM\n123-456"

# Test 10: Large input
large_input=""
for i in {1..100}; do
    large_input="${large_input}line$i\n"
done
large_input="${large_input}<END>"

echo -n "Test $((TESTS + 1)): Large input (100 lines)... "
TESTS=$((TESTS + 1))
lines_processed=$(echo -e "$large_input" | $PIPELINE $PLUGIN_DIR/upper.so 2>/dev/null | grep -v "^Loaded plugin:" | wc -l)
if [ "$lines_processed" -eq 100 ]; then
    echo -e "${GREEN}PASSED${NC}"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}FAILED${NC}"
    echo "  Expected 100 lines, got $lines_processed"
    FAILED=$((FAILED + 1))
fi

# Summary
echo ""
echo "=============================="
echo "End-to-End Test Summary"
echo "=============================="
echo "Total tests: $TESTS"
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo -e "\n${GREEN}✅ All E2E tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}❌ Some E2E tests failed!${NC}"
    exit 1
fi