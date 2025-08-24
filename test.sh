#!/bin/bash

# Test script for String Processing Pipeline
# Runs all test suites: unit tests, integration tests, and E2E tests

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}      String Processing Pipeline - Test Suite           ${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"

# Check if build exists
if [ ! -d "build/bin" ]; then
    echo -e "${YELLOW}Build directory not found. Running build.sh...${NC}"
    ./build.sh
fi

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Run mode (unit, integration, e2e, all)
MODE=${1:-all}

# ==========================
# UNIT TESTS
# ==========================
run_unit_tests() {
    echo -e "\n${YELLOW}════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}                    UNIT TESTS                          ${NC}"
    echo -e "${YELLOW}════════════════════════════════════════════════════════${NC}"
    
    # Queue Unit Tests
    if [ -f "build/bin/test_queue" ]; then
        echo -e "\n${GREEN}Running Queue Unit Tests...${NC}"
        if ./build/bin/test_queue > /tmp/queue_test.log 2>&1; then
            queue_passed=$(grep -c "✓ PASSED" /tmp/queue_test.log || echo "0")
            queue_total=$(grep "Total tests run:" /tmp/queue_test.log | awk '{print $4}' || echo "0")
            if grep -q "ALL TESTS PASSED" /tmp/queue_test.log; then
                echo -e "${GREEN}  ✅ Queue Tests: $queue_passed/$queue_total passed${NC}"
                PASSED_TESTS=$((PASSED_TESTS + queue_passed))
            else
                queue_failed=$(grep -c "❌ FAILED" /tmp/queue_test.log || echo "0")
                echo -e "${RED}  ❌ Queue Tests: $queue_failed tests failed${NC}"
                grep "❌ FAILED" /tmp/queue_test.log | head -3
                FAILED_TESTS=$((FAILED_TESTS + queue_failed))
            fi
            TOTAL_TESTS=$((TOTAL_TESTS + queue_total))
        else
            echo -e "${RED}  ❌ Queue tests crashed${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            TOTAL_TESTS=$((TOTAL_TESTS + 1))
        fi
    fi
    
    # Monitor Unit Tests
    if [ -f "build/bin/test_monitor" ]; then
        echo -e "\n${GREEN}Running Monitor Unit Tests...${NC}"
        if ./build/bin/test_monitor > /tmp/monitor_test.log 2>&1; then
            monitor_passed=$(grep -c "✓ PASSED" /tmp/monitor_test.log || echo "0")
            monitor_total=$(grep "Total tests run:" /tmp/monitor_test.log | awk '{print $4}' || echo "0")
            if grep -q "ALL TESTS PASSED" /tmp/monitor_test.log; then
                echo -e "${GREEN}  ✅ Monitor Tests: $monitor_passed/$monitor_total passed${NC}"
                PASSED_TESTS=$((PASSED_TESTS + monitor_passed))
            else
                monitor_failed=$(grep -c "❌ FAILED" /tmp/monitor_test.log || echo "0")
                echo -e "${RED}  ❌ Monitor Tests: $monitor_failed tests failed${NC}"
                grep "❌ FAILED" /tmp/monitor_test.log | head -3
                FAILED_TESTS=$((FAILED_TESTS + monitor_failed))
            fi
            TOTAL_TESTS=$((TOTAL_TESTS + monitor_total))
        else
            echo -e "${RED}  ❌ Monitor tests crashed${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            TOTAL_TESTS=$((TOTAL_TESTS + 1))
        fi
    fi
}

# ==========================
# INTEGRATION TESTS
# ==========================
run_integration_tests() {
    echo -e "\n${YELLOW}════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}                 INTEGRATION TESTS                       ${NC}"
    echo -e "${YELLOW}════════════════════════════════════════════════════════${NC}"
    
    local int_passed=0
    local int_failed=0
    
    # Test individual plugins
    echo -e "\n${GREEN}Testing Individual Plugins...${NC}"
    for plugin in upper lower reverse trim prefix suffix; do
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        echo -n "  Testing $plugin plugin... "
        
        result=$(echo -e "test\n<END>" | ./build/bin/pipeline ./build/lib/plugins/${plugin}.so 2>/dev/null | grep -v "^Loaded" || echo "ERROR")
        
        case $plugin in
            upper)   expected="TEST" ;;
            lower)   expected="test" ;;
            reverse) expected="tset" ;;
            trim)    expected="test" ;;
            prefix)  expected="PREFIX:test" ;;
            suffix)  expected="test:SUFFIX" ;;
        esac
        
        if [ "$result" = "$expected" ]; then
            echo -e "${GREEN}✅${NC}"
            int_passed=$((int_passed + 1))
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            echo -e "${RED}❌ (expected: $expected, got: $result)${NC}"
            int_failed=$((int_failed + 1))
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    done
    
    # Test plugin combinations
    echo -e "\n${GREEN}Testing Plugin Combinations...${NC}"
    
    # Test 1: Two plugins
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -n "  upper → reverse: "
    result=$(echo -e "hello\n<END>" | ./build/bin/pipeline ./build/lib/plugins/upper.so ./build/lib/plugins/reverse.so 2>/dev/null | grep -v "^Loaded" || echo "ERROR")
    if [ "$result" = "OLLEH" ]; then
        echo -e "${GREEN}✅${NC}"
        int_passed=$((int_passed + 1))
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}❌ (expected: OLLEH, got: $result)${NC}"
        int_failed=$((int_failed + 1))
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    # Test 2: Three plugins
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -n "  trim → upper → prefix: "
    result=$(echo -e "  test  \n<END>" | ./build/bin/pipeline ./build/lib/plugins/trim.so ./build/lib/plugins/upper.so ./build/lib/plugins/prefix.so 2>/dev/null | grep -v "^Loaded" || echo "ERROR")
    if [ "$result" = "PREFIX:TEST" ]; then
        echo -e "${GREEN}✅${NC}"
        int_passed=$((int_passed + 1))
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}❌ (expected: PREFIX:TEST, got: $result)${NC}"
        int_failed=$((int_failed + 1))
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    echo -e "${BLUE}  Integration Tests: $int_passed passed, $int_failed failed${NC}"
}

# ==========================
# END-TO-END TESTS
# ==========================
run_e2e_tests() {
    echo -e "\n${YELLOW}════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}                  END-TO-END TESTS                       ${NC}"
    echo -e "${YELLOW}════════════════════════════════════════════════════════${NC}"
    
    local e2e_passed=0
    local e2e_failed=0
    
    run_e2e_test() {
        local name="$1"
        local input="$2"
        local plugins="$3"
        local expected="$4"
        
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        echo -n "  $name... "
        
        actual=$(echo -e "$input" | ./build/bin/pipeline $plugins 2>/dev/null | grep -v "^Loaded plugin:" | tr '\n' '|' | sed 's/|$//')
        expected_formatted=$(echo -e "$expected" | tr '\n' '|' | sed 's/|$//')
        
        if [ "$actual" = "$expected_formatted" ]; then
            echo -e "${GREEN}✅${NC}"
            e2e_passed=$((e2e_passed + 1))
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            echo -e "${RED}❌${NC}"
            echo "    Expected: $expected_formatted"
            echo "    Got:      $actual"
            e2e_failed=$((e2e_failed + 1))
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    }
    
    echo ""
    
    # E2E Test Cases
    run_e2e_test "Single line" \
        "hello\n<END>" \
        "./build/lib/plugins/upper.so" \
        "HELLO"
    
    run_e2e_test "Multiple lines" \
        "hello\nworld\n<END>" \
        "./build/lib/plugins/upper.so" \
        "HELLO\nWORLD"
    
    run_e2e_test "Pipeline chain" \
        "hello\n<END>" \
        "./build/lib/plugins/upper.so ./build/lib/plugins/reverse.so" \
        "OLLEH"
    
    run_e2e_test "Empty input" \
        "<END>" \
        "./build/lib/plugins/upper.so" \
        ""
    
    run_e2e_test "Special characters" \
        "hello@world.com\n123-456\n<END>" \
        "./build/lib/plugins/upper.so" \
        "HELLO@WORLD.COM\n123-456"
    
    run_e2e_test "All six plugins" \
        "  hello  \n<END>" \
        "./build/lib/plugins/trim.so ./build/lib/plugins/upper.so ./build/lib/plugins/reverse.so ./build/lib/plugins/prefix.so ./build/lib/plugins/suffix.so ./build/lib/plugins/lower.so" \
        "prefix:olleh:suffix"
    
    # Large input test
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -n "  Large input (1000 lines)... "
    large_input=""
    for i in {1..1000}; do
        large_input="${large_input}line$i\n"
    done
    large_input="${large_input}<END>"
    
    lines_processed=$(echo -e "$large_input" | ./build/bin/pipeline ./build/lib/plugins/upper.so 2>/dev/null | grep -v "^Loaded plugin:" | wc -l)
    if [ "$lines_processed" -eq 1000 ]; then
        echo -e "${GREEN}✅${NC}"
        e2e_passed=$((e2e_passed + 1))
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}❌ (expected 1000 lines, got $lines_processed)${NC}"
        e2e_failed=$((e2e_failed + 1))
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    echo -e "${BLUE}  E2E Tests: $e2e_passed passed, $e2e_failed failed${NC}"
}

# ==========================
# MEMORY TESTS (if valgrind available)
# ==========================
run_memory_tests() {
    echo -e "\n${YELLOW}════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}                    MEMORY TESTS                         ${NC}"
    echo -e "${YELLOW}════════════════════════════════════════════════════════${NC}"
    
    if command -v valgrind &> /dev/null; then
        echo -e "\n${GREEN}Running Valgrind Memory Tests...${NC}"
        
        echo -n "  Queue tests memory check... "
        if valgrind --leak-check=full --error-exitcode=42 --quiet \
                   ./build/bin/test_queue > /dev/null 2>&1; then
            echo -e "${GREEN}✅ No memory leaks${NC}"
        else
            echo -e "${RED}❌ Memory leaks detected${NC}"
        fi
        
        echo -n "  Monitor tests memory check... "
        if valgrind --leak-check=full --error-exitcode=42 --quiet \
                   ./build/bin/test_monitor > /dev/null 2>&1; then
            echo -e "${GREEN}✅ No memory leaks${NC}"
        else
            echo -e "${RED}❌ Memory leaks detected${NC}"
        fi
    else
        echo -e "${YELLOW}  Valgrind not available - skipping memory tests${NC}"
    fi
}

# ==========================
# PERFORMANCE TEST
# ==========================
run_performance_test() {
    echo -e "\n${YELLOW}════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}                  PERFORMANCE TEST                       ${NC}"
    echo -e "${YELLOW}════════════════════════════════════════════════════════${NC}"
    
    echo -n "  Processing 10000 lines through 3 plugins... "
    start_time=$(date +%s 2>/dev/null || echo "0")
    
    for i in {1..10000}; do echo "line$i"; done | \
        (cat; echo "<END>") | \
        ./build/bin/pipeline ./build/lib/plugins/upper.so ./build/lib/plugins/reverse.so ./build/lib/plugins/lower.so 2>/dev/null | \
        wc -l > /tmp/perf_test.txt 2>&1
    
    end_time=$(date +%s 2>/dev/null || echo "0")
    elapsed=$((end_time - start_time))
    lines=$(cat /tmp/perf_test.txt)
    
    if [ "$lines" -gt 9000 ]; then
        echo -e "${GREEN}✅ Processed ~$lines lines in ~${elapsed}ms${NC}"
    else
        echo -e "${RED}❌ Only processed $lines lines${NC}"
    fi
}

# ==========================
# MAIN TEST EXECUTION
# ==========================
case $MODE in
    unit)
        run_unit_tests
        ;;
    integration)
        run_integration_tests
        ;;
    e2e)
        run_e2e_tests
        ;;
    memory)
        run_memory_tests
        ;;
    performance)
        run_performance_test
        ;;
    all)
        run_unit_tests
        run_integration_tests
        run_e2e_tests
        run_memory_tests
        run_performance_test
        ;;
    *)
        echo -e "${RED}Unknown mode: $MODE${NC}"
        echo "Usage: $0 [unit|integration|e2e|memory|performance|all]"
        exit 1
        ;;
esac

# ==========================
# FINAL SUMMARY
# ==========================
echo -e "\n${CYAN}════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}                    TEST SUMMARY                         ${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"

echo -e "\nTotal Tests Run: $TOTAL_TESTS"
echo -e "${GREEN}Tests Passed: $PASSED_TESTS${NC}"
echo -e "${RED}Tests Failed: $FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ] && [ $TOTAL_TESTS -gt 0 ]; then
    echo -e "\n${GREEN}╔════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║        ✅ ALL TESTS PASSED! ✅             ║${NC}"
    echo -e "${GREEN}║    Pipeline is production ready!           ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════╝${NC}"
    exit 0
else
    if [ $TOTAL_TESTS -eq 0 ]; then
        echo -e "\n${YELLOW}⚠️  No tests were run${NC}"
    else
        echo -e "\n${RED}╔════════════════════════════════════════════╗${NC}"
        echo -e "${RED}║        ❌ SOME TESTS FAILED ❌             ║${NC}"
        echo -e "${RED}║       Please review failures above         ║${NC}"
        echo -e "${RED}╚════════════════════════════════════════════╝${NC}"
    fi
    exit 1
fi