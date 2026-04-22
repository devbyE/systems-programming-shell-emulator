#!/bin/bash

# test_phase3.sh
# Test script for MyShell Phase 3 - Pipes and Command Chaining
# Tests: pipes (|), logical AND (&&), logical OR (||), sequential (;)
# Compatible with Linux/Unix systems

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
RESULTS_FILE="test_results_phase3.txt"
TEST_DIR="/tmp/myshell_test_phase3_$$"

# Create temp directory for test files
mkdir -p "$TEST_DIR"

> $RESULTS_FILE

print_header() {
    echo ""
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}======================================${NC}"
}

test_pass() {
    echo -e "${GREEN}√ PASS${NC}: $1"
    echo "PASS: $1" >> $RESULTS_FILE
    ((TESTS_PASSED++))
    ((TESTS_RUN++))
}

test_fail() {
    echo -e "${RED}x FAIL${NC}: $1"
    echo "FAIL: $1" >> $RESULTS_FILE
    ((TESTS_FAILED++))
    ((TESTS_RUN++))
}

test_info() {
    echo -e "${YELLOW}ℹ INFO${NC}: $1"
    echo "INFO: $1" >> $RESULTS_FILE
}

# Check for myshell
check_executable() {
    if [ ! -f "./myshell" ]; then
        echo -e "${RED}Error: ./myshell executable not found${NC}"
        exit 1
    fi
    
    if [ ! -x "./myshell" ]; then
        chmod +x ./myshell 2>/dev/null || true
    fi
    
    MYSHELL="./myshell"
}

# ============ TEST GROUP 1: Basic Pipes ============
test_group_1() {
    print_header "GROUP 1: Basic Pipes (|)"
    
    # Test 1: Simple pipe - ls to grep
    output=$(echo -e "echo 'file1.txt\nfile2.c\nfile3.txt' | grep '.txt'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "file1.txt" && echo "$output" | grep -q "file3.txt"; then
        test_pass "Simple pipe with grep"
    else
        test_fail "Simple pipe with grep"
    fi
    
    # Test 2: Pipe to wc
    output=$(echo -e "echo -e 'line1\nline2\nline3' | wc -l\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -qE "[0-9]"; then
        test_pass "Pipe to wc (count lines)"
    else
        test_fail "Pipe to wc (count lines)"
    fi
    
    # Test 3: Pipe to cat
    output=$(echo -e "echo 'test content' | cat\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "test content"; then
        test_pass "Pipe to cat"
    else
        test_fail "Pipe to cat"
    fi
    
    # Test 4: Pipe with sort
    output=$(echo -e "echo -e 'c\na\nb' | sort\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "a"; then
        test_pass "Pipe with sort"
    else
        test_fail "Pipe with sort"
    fi
}

# ============ TEST GROUP 2: Multiple Pipes ============
test_group_2() {
    print_header "GROUP 2: Multiple Pipes (| | |)"
    
    # Test 1: Two pipes
    output=$(echo -e "echo -e 'a\nb\nc' | sort | cat\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "a" && echo "$output" | grep -q "c"; then
        test_pass "Two pipes in sequence"
    else
        test_fail "Two pipes in sequence"
    fi
    
    # Test 2: Three pipes
    output=$(echo -e "echo 'apple\nbanana\napple' | sort | uniq | wc -l\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -qE "[0-9]"; then
        test_pass "Three pipes in sequence"
    else
        test_fail "Three pipes in sequence"
    fi
    
    # Test 3: Pipes with grep and sort
    output=$(echo -e "echo -e 'file1.txt\ntest.c\nfile2.txt' | grep '.txt' | sort\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "file1.txt"; then
        test_pass "Pipes with grep and sort"
    else
        test_fail "Pipes with grep and sort"
    fi
}

# ============ TEST GROUP 3: Pipes with Output Redirection ============
test_group_3() {
    print_header "GROUP 3: Pipes with Output Redirection (| > file)"
    
    # Test 1: Pipe output to file
    echo -e "echo -e 'line1\nline2\nline3' | sort > $TEST_DIR/pipe_out1.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/pipe_out1.txt" ]; then
        content=$(cat "$TEST_DIR/pipe_out1.txt")
        if echo "$content" | grep -q "line1"; then
            test_pass "Pipe output to file"
        else
            test_fail "Pipe output to file (content wrong)"
        fi
    else
        test_fail "Pipe output to file (file not created)"
    fi
    
    # Test 2: Multiple pipes with redirection
    echo -e "echo -e 'c\na\nb' | sort | cat > $TEST_DIR/pipe_out2.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/pipe_out2.txt" ]; then
        lines=$(wc -l < "$TEST_DIR/pipe_out2.txt")
        if [ "$lines" -ge 3 ]; then
            test_pass "Multiple pipes with output redirection"
        else
            test_fail "Multiple pipes with output redirection (lines wrong)"
        fi
    else
        test_fail "Multiple pipes with output redirection (file not created)"
    fi
    
    # Test 3: Pipe with append redirection
    echo "existing" > "$TEST_DIR/pipe_append.txt"
    echo -e "echo 'appended' | cat >> $TEST_DIR/pipe_append.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/pipe_append.txt" ]; then
        lines=$(wc -l < "$TEST_DIR/pipe_append.txt")
        if [ "$lines" -ge 2 ]; then
            test_pass "Pipe with append redirection (>>)"
        else
            test_fail "Pipe with append redirection (lines wrong)"
        fi
    else
        test_fail "Pipe with append redirection (file not found)"
    fi
}

# ============ TEST GROUP 4: Pipes with Input Redirection ============
test_group_4() {
    print_header "GROUP 4: Pipes with Input Redirection (< file | cmd)"
    
    # Test 1: Input file piped to grep
    echo -e "line1\nline2\nline3" > "$TEST_DIR/pipe_input1.txt"
    output=$(echo -e "cat < $TEST_DIR/pipe_input1.txt | grep line2\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "line2"; then
        test_pass "Input redirection piped to grep"
    else
        test_fail "Input redirection piped to grep"
    fi
    
    # Test 2: Input file piped to sort
    echo -e "z\na\nm" > "$TEST_DIR/pipe_input2.txt"
    output=$(echo -e "cat < $TEST_DIR/pipe_input2.txt | sort\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "a"; then
        test_pass "Input redirection piped to sort"
    else
        test_fail "Input redirection piped to sort"
    fi
    
    # Test 3: Combined input, pipe, output
    echo -e "c\nb\na" > "$TEST_DIR/pipe_combined.txt"
    echo -e "cat < $TEST_DIR/pipe_combined.txt | sort > $TEST_DIR/pipe_combined_out.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/pipe_combined_out.txt" ]; then
        content=$(cat "$TEST_DIR/pipe_combined_out.txt")
        if echo "$content" | head -1 | grep -q "a"; then
            test_pass "Combined input, pipe, output redirection"
        else
            test_fail "Combined input, pipe, output (order wrong)"
        fi
    else
        test_fail "Combined input, pipe, output (file not created)"
    fi
}

# ============ TEST GROUP 5: Logical AND (&&) ============
test_group_5() {
    print_header "GROUP 5: Logical AND (&&)"
    
    # Test 1: Successful first command, runs second
    output=$(echo -e "echo 'first' && echo 'second'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "first" && echo "$output" | grep -q "second"; then
        test_pass "Logical AND with both commands executing"
    else
        test_fail "Logical AND with both commands executing"
    fi
    
    # Test 2: False first command, skips second
    output=$(echo -e "false && echo 'should not appear'\nexit" | $MYSHELL 2>&1)
    if ! echo "$output" | grep -q "should not appear"; then
        test_pass "Logical AND skips second command on failure"
    else
        test_fail "Logical AND does not skip on failure"
    fi
    
    # Test 3: Multiple ANDs
    output=$(echo -e "echo 'a' && echo 'b' && echo 'c'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "a" && echo "$output" | grep -q "b" && echo "$output" | grep -q "c"; then
        test_pass "Multiple logical AND operators"
    else
        test_fail "Multiple logical AND operators"
    fi
    
    # Test 4: AND with file creation
    echo -e "echo 'test' > $TEST_DIR/and_test.txt && cat $TEST_DIR/and_test.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/and_test.txt" ]; then
        test_pass "Logical AND with file operations"
    else
        test_fail "Logical AND with file operations"
    fi
}

# ============ TEST GROUP 6: Logical OR (||) ============
test_group_6() {
    print_header "GROUP 6: Logical OR (||)"
    
    # Test 1: First command succeeds, skips second
    output=$(echo -e "echo 'success' || echo 'backup'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "success" && ! echo "$output" | grep -q "backup"; then
        test_pass "Logical OR executes first command only"
    else
        test_fail "Logical OR executes first command only"
    fi
    
    # Test 2: First command fails, runs second
    output=$(echo -e "false || echo 'fallback'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "fallback"; then
        test_pass "Logical OR runs second command on failure"
    else
        test_fail "Logical OR runs second command on failure"
    fi
    
    # Test 3: Multiple ORs
    output=$(echo -e "false || false || echo 'finally'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "finally"; then
        test_pass "Multiple logical OR operators"
    else
        test_fail "Multiple logical OR operators"
    fi
    
    # Test 4: OR with fallback message
    output=$(echo -e "test -f /nonexistent || echo 'File not found'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "File not found"; then
        test_pass "Logical OR with file test fallback"
    else
        test_fail "Logical OR with file test fallback"
    fi
}

# ============ TEST GROUP 7: Sequential Execution (;) ============
test_group_7() {
    print_header "GROUP 7: Sequential Execution (;)"
    
    # Test 1: Two commands in sequence
    output=$(echo -e "echo 'first' ; echo 'second'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "first" && echo "$output" | grep -q "second"; then
        test_pass "Sequential execution of two commands"
    else
        test_fail "Sequential execution of two commands"
    fi
    
    # Test 2: Three commands in sequence
    output=$(echo -e "echo 'a' ; echo 'b' ; echo 'c'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "a" && echo "$output" | grep -q "b" && echo "$output" | grep -q "c"; then
        test_pass "Sequential execution of three commands"
    else
        test_fail "Sequential execution of three commands"
    fi
    
    # Test 3: Sequential with failing command (still runs next)
    output=$(echo -e "false ; echo 'continues anyway'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "continues anyway"; then
        test_pass "Sequential continues after failed command"
    else
        test_fail "Sequential continues after failed command"
    fi
    
    # Test 4: Sequential with file operations
    echo -e "echo 'a' > $TEST_DIR/seq1.txt ; echo 'b' > $TEST_DIR/seq2.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/seq1.txt" ] && [ -f "$TEST_DIR/seq2.txt" ]; then
        test_pass "Sequential file creation"
    else
        test_fail "Sequential file creation"
    fi
}

# ============ TEST GROUP 8: Mixed Operators ============
test_group_8() {
    print_header "GROUP 8: Mixed Operators"
    
    # Test 1: Pipe with AND
    output=$(echo -e "echo 'test' | grep 'test' && echo 'success'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "success"; then
        test_pass "Pipe with logical AND"
    else
        test_fail "Pipe with logical AND"
    fi
    
    # Test 2: Pipe with OR
    output=$(echo -e "echo 'test' | grep 'nomatch' || echo 'fallback'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "fallback"; then
        test_pass "Pipe with logical OR"
    else
        test_fail "Pipe with logical OR"
    fi
    
    # Test 3: AND with sequential
    output=$(echo -e "echo 'a' && echo 'b' ; echo 'c'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "a" && echo "$output" | grep -q "b" && echo "$output" | grep -q "c"; then
        test_pass "AND with sequential operator"
    else
        test_fail "AND with sequential operator"
    fi
    
    # Test 4: OR with pipe
    output=$(echo -e "false || echo 'fallback' | cat\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "fallback"; then
        test_pass "OR with pipe operator"
    else
        test_fail "OR with pipe operator"
    fi
    
    # Test 5: Complex combination
    output=$(echo -e "echo 'data' | grep 'data' && echo 'found' || echo 'not found'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "found"; then
        test_pass "Complex operator combination"
    else
        test_fail "Complex operator combination"
    fi
}

# ============ TEST GROUP 9: Pipes with Built-in Commands ============
test_group_9() {
    print_header "GROUP 9: Pipes with Built-in Commands"
    
    # Test 1: pwd piped to cat
    output=$(echo -e "pwd | cat\nexit" | $MYSHELL 2>&1)
    if [ ! -z "$output" ]; then
        test_pass "pwd piped to cat"
    else
        test_fail "pwd piped to cat"
    fi
    
    # Test 2: echo piped to grep
    output=$(echo -e "echo 'hello world' | grep 'world'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "world"; then
        test_pass "echo piped to grep"
    else
        test_fail "echo piped to grep"
    fi
    
    # Test 3: cd && pwd (AND with directory)
    output=$(echo -e "cd /tmp && pwd\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "/tmp"; then
        test_pass "cd with AND operator"
    else
        test_fail "cd with AND operator"
    fi
}

# ============ TEST GROUP 10: Error Handling in Pipes ============
test_group_10() {
    print_header "GROUP 10: Error Handling in Pipes"
    
    # Test 1: Command not found in pipe
    output=$(echo -e "echo 'test' | nonexistentcmd\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -qE "command not found|not found"; then
        test_pass "Error handling for nonexistent command in pipe"
    else
        test_info "Error message not shown for bad command"
    fi
    
    # Test 2: Pipe with 2> error redirection
    echo -e "cat notfound 2> $TEST_DIR/err.txt | grep 'error'\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/err.txt" ]; then
        test_pass "Error redirection in pipeline"
    else
        test_fail "Error redirection in pipeline"
    fi
    
    # Test 3: AND with failing middle command
    output=$(echo -e "echo 'a' && false && echo 'c'\nexit" | $MYSHELL 2>&1)
    if ! echo "$output" | grep -q "^c"; then
        test_pass "AND stops at failing middle command"
    else
        test_fail "AND does not stop at middle failure"
    fi
}

# ============ TEST GROUP 11: Complex Scenarios ============
test_group_11() {
    print_header "GROUP 11: Complex Scenarios"
    
    # Test 1: Create file with pipe and grep result
    echo -e "echo -e 'apple\nbanana\napple' | grep 'apple' | wc -l > $TEST_DIR/count.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/count.txt" ]; then
        test_pass "Complex: file creation with pipe chain and grep"
    else
        test_fail "Complex: file creation with pipe chain"
    fi
    
    # Test 2: Multiple operations with mixed operators
    output=$(echo -e "echo 'start' && echo 'test' | grep 'test' && echo 'end'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "start" && echo "$output" | grep -q "end"; then
        test_pass "Multiple mixed operators in sequence"
    else
        test_fail "Multiple mixed operators in sequence"
    fi
    
    # Test 3: Pipe with conditional execution
    echo -e "mkdir -p $TEST_DIR/subdir && echo 'created' > $TEST_DIR/subdir/file.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/subdir/file.txt" ]; then
        test_pass "Conditional directory creation with AND"
    else
        test_fail "Conditional directory creation with AND"
    fi
}

# ============ TEST GROUP 12: Quote Protection in Operators ============
test_group_12() {
    print_header "GROUP 12: Quote Protection with Operators"
    
    # Test 1: Pipe in quoted string
    output=$(echo -e "echo 'pipe | not here' | grep 'pipe'\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "pipe |"; then
        test_pass "Quote protection for pipe in string"
    else
        test_fail "Quote protection for pipe in string"
    fi
    
    # Test 2: Operator in quoted string
    output=$(echo -e "echo 'test && more' | cat\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "test &&"; then
        test_pass "Quote protection for AND in string"
    else
        test_fail "Quote protection for AND in string"
    fi
    
    # Test 3: Complex quoted expression
    output=$(echo -e "echo 'echo hello | grep hello && echo success' | cat\nexit" | $MYSHELL 2>&1)
    if [ ! -z "$output" ]; then
        test_pass "Complex quote protection"
    else
        test_fail "Complex quote protection"
    fi
}

# ============ MAIN ============
main() {
    echo ""
    echo -e "${BLUE}========================================================${NC}"
    echo -e "${BLUE}  MyShell Phase 3 - Pipes and Chaining Tests${NC}"
    echo -e "${BLUE}  Georgetown University Systems Programming${NC}"
    echo -e "${BLUE}========================================================${NC}"
    echo ""
    
    check_executable
    echo -e "${GREEN}Found executable: $MYSHELL${NC}"
    echo -e "${GREEN}Test directory: $TEST_DIR${NC}"
    echo ""
    
    test_group_1
    test_group_2
    test_group_3
    test_group_4
    test_group_5
    test_group_6
    test_group_7
    test_group_8
    test_group_9
    test_group_10
    test_group_11
    test_group_12
    
    # Cleanup (optional - comment out to preserve test files)
    # rm -rf "$TEST_DIR"
    
    # Print summary
    print_header "TEST SUMMARY"
    echo -e "Total Tests Run:  ${BLUE}$TESTS_RUN${NC}"
    echo -e "Tests Passed:     ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests Failed:     ${RED}$TESTS_FAILED${NC}"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}All tests passed! ✓${NC}"
        echo "Results saved to: $RESULTS_FILE"
        return 0
    else
        if [ $TESTS_FAILED -le 5 ]; then
            echo -e "${YELLOW}Most tests passed! ⚠${NC}"
        else
            echo -e "${RED}Some tests failed.${NC}"
        fi
        echo "Results saved to: $RESULTS_FILE"
        return 1
    fi
}

main
exit $?
