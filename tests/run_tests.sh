#!/bin/bash
# Integration tests for txtFormatter
# Usage: ./run_tests.sh
# Returns 0 if all tests pass, 1 otherwise

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TEST_DIR=$(mktemp -d)
PASS=0
FAIL=0
TOTAL=0

cleanup() {
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

run_test() {
    local test_name="$1"
    local input="$2"
    local expected="$3"

    TOTAL=$((TOTAL + 1))

    # Write input file
    printf '%s' "$input" > "$TEST_DIR/test.txtm"

    # Run txtfmt in test directory
    (cd "$TEST_DIR" && "$SCRIPT_DIR/../txtfmt" > /dev/null 2>&1) || true

    # Check output
    if [ -f "$TEST_DIR/test.txt" ]; then
        # Strip trailing whitespace per line for comparison
        actual=$(sed 's/[[:space:]]*$//' "$TEST_DIR/test.txt")
        expected_clean=$(printf '%s' "$expected" | sed 's/[[:space:]]*$//')
        if [ "$actual" = "$expected_clean" ]; then
            PASS=$((PASS + 1))
            echo "  PASS: $test_name"
        else
            FAIL=$((FAIL + 1))
            echo "  FAIL: $test_name"
            echo "    Expected:"
            echo "$expected_clean" | head -5 | sed 's/^/      /'
            echo "    Got:"
            echo "$actual" | head -5 | sed 's/^/      /'
        fi
    else
        FAIL=$((FAIL + 1))
        echo "  FAIL: $test_name (no output file)"
    fi

    rm -f "$TEST_DIR/test.txt" "$TEST_DIR/test.txtm"
}

echo "============================================"
echo " txtFormatter Integration Tests"
echo "============================================"
echo ""

# Test 1: Simple paragraph
echo "--- Basic Tags ---"
run_test "Simple paragraph" \
"<p>Hello world</p>" \
"  Hello world"

# Test 2: Header h1
run_test "Header h1" \
"<h1>Test</h1>" \
"================================================================================
                                      Test                                      
================================================================================"

# Test 3: Header h4
run_test "Header h4" \
"<h4>Subtitle</h4>" \
"Subtitle
--------"

# Test 4: Separator
run_test "Separator" \
"<sep>" \
"--------------------------------------------------------------------------------"

# Test 5: Date
run_test "Date tag" \
"<date>" \
"$(date +%d.%m.%Y)"

# Test 6: Time
run_test "Time tag" \
"<time>" \
"$(date +%H:%M:%S)"

# Test 7: Numbered list
echo ""
echo "--- Lists ---"
run_test "Numbered list" \
"<list>A
B
C</list>" \
" 1) A
 2) B
 3) C"

# Test 8: Bulleted list
run_test "Bulleted list" \
"<list *>X
Y</list>" \
" * X
 * Y"

# Test 9: Centered text
echo ""
echo "--- Alignment ---"
# Center pads to DOC_WIDTH (80 chars): (80-2)/2 = 39 spaces left, 39 right
run_test "Center single line" \
"<center>Hi</center>" \
"                                       Hi"

# Test 10: Right aligned
run_test "Right single line" \
"<right>End</right>" \
"                                                                             End"

# Test 11: Calc
echo ""
echo "--- Calculations ---"
run_test "Simple calc" \
"<calc>2+2</calc>" \
"4"

# Test 12: Calc with expression
run_test "Calc with expression" \
"<calc s>10*10</calc>" \
"10*10 = 100"

# Test 13: Multiple calcs
run_test "Multiple calcs" \
"<calc>1+1
2+2
3+3</calc>" \
"2
4
6"

# Test 14: Lines
echo ""
echo "--- Utilities ---"
run_test "Empty lines" \
"Before<lines 3>After" \
"Before


After"

# Test 15: Custom separator
run_test "Custom separator" \
"<sep *>" \
"********************************************************************************"

# Test 16: Nested tags
echo ""
echo "--- Nesting ---"
# center(calc) — calc returns "144", center pads to 80 chars
run_test "Nested center+calc" \
"<center><calc>12*12</calc></center>" \
"                                      144                                       "

# Test 17: Frame
run_test "Simple frame" \
"<frame>test</frame>" \
" .+-====-+. 
 ||   test   || 
 '+-====-+' "

# Test 18: Table without border
echo ""
echo "--- Tables ---"
run_test "Table no border" \
"<table nb>A|B
C|D</table>" \
"A                                      B                                       
C                                      D                                       
"

# Test 19: Document width
echo ""
echo "--- Document Width ---"
run_test "Set width 40" \
"<doc_width 40><sep>" \
"----------------------------------------"

# Test 20: Default width
run_test "Reset default width" \
"<doc_width 20><default_width><sep>" \
"--------------------------------------------------------------------------------"

# Test 21: Empty tag (no content between open and close)
echo ""
echo "--- Edge Cases ---"
run_test "Empty tag returns empty string" \
"<p></p>" \
""

# Test 22: Deep nesting (3 levels)
# Note: <calc> returns "4", <center> centers it to DOC_WIDTH (80 chars)
run_test "Triple nesting" \
"<center><calc>2+2</calc></center>" \
"                                       4                                        "

# Test 23: Multiple tags on same line
run_test "Multiple tags inline" \
"<date> <time>" \
"$(date +%d.%m.%Y) $(date +%H:%M:%S)"

# Test 24: Separator with custom character
run_test "Separator equals sign" \
"<sep =>" \
"================================================================================"

# Test 25: Frame with empty content
run_test "Empty frame" \
"<frame></frame>" \
""

# Test 26: Calc error handling (division by zero)
run_test "Calc division by zero" \
"<calc>1/0</calc>" \
"error"

# Test 27: Table without border and without calculations
run_test "Table no calc no border" \
"<table nb nc>Hello|World
Foo|Bar</table>" \
"Hello                                  World
Foo                                    Bar
"

# Test 28: Bulleted list with hash default
run_test "Hash bullet list" \
"<list #>X
Y</list>" \
" # X
 # Y"

# Test 29: Lines with zero (0 is not > 0, falls back to default count=1 → single \n)
# Result: "Before\nAfter" — no blank line between
run_test "Lines zero falls back to default" \
"Before<lines 0>After" \
"Before
After"

# Test 30: Unclosed tag — content after opening tag is treated as tag content
# <p>Hello without </p> → "Hello" is processed by <p> tag (adds 2-space indent)
run_test "Unclosed tag processes content" \
"Before<p>Hello" \
"Before  Hello"

# Summary
echo ""
echo "============================================"
echo " Results: $PASS passed, $FAIL failed, $TOTAL total"
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
