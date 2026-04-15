#!/bin/bash
#
# txtfmt stress testing — maximum severity checks
#
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TXTFMT="$SCRIPT_DIR/../txtfmt"
BASE_TEST_DIR=$(mktemp -d)
TOTAL=0
PASSED=0
FAILED=0
CRASHED=0

# Arrays for report
declare -a FAIL_NAMES=()
declare -a FAIL_REASONS=()

cleanup() {
    echo ""
    echo "Working directory saved: $BASE_TEST_DIR"
}
trap cleanup EXIT

# ---------- Utilities ----------

run_single_test() {
    local test_name="$1"
    local input_content="$2"     # content of the .txtm file
    local expect_exit0="${3:-1}" # 1 = expect exit 0, 0 = allow error
    local is_binary="${4:-0}"    # 1 = write via python (binary-safe)

    TOTAL=$((TOTAL + 1))
    local test_dir="$BASE_TEST_DIR/test_$TOTAL"
    mkdir -p "$test_dir"

    local input_file="$test_dir/input.txtm"
    local output_file="$test_dir/output.txt"
    local stderr_file="$test_dir/stderr.txt"

    if [[ "$is_binary" == "1" ]]; then
        # input_content already contains binary data via python
        : # file already created externally
    else
        printf '%s' "$input_content" > "$input_file"
    fi

    # Run in a separate directory
    local exit_code=0
    (cd "$test_dir" && "$TXTFMT" > "$test_dir/stdout.txt" 2>"$stderr_file") || exit_code=$?

    local fail_reason=""

    # 1. Check exit code
    if [[ "$expect_exit0" == "1" ]] && [[ $exit_code -ne 0 ]]; then
        fail_reason+="exit_code=$exit_code "
    fi

    # 2. Check that .txt file was created (if exit_code == 0)
    if [[ $exit_code -eq 0 ]] && [[ ! -f "$test_dir/input.txt" ]]; then
        fail_reason+="no_output_txt "
    fi

    # 3. Check for crash messages in stderr
    if [[ -f "$stderr_file" ]]; then
        if grep -qi "segmentation fault" "$stderr_file" 2>/dev/null; then
            fail_reason+="SEGFAULT "
        fi
        if grep -qi "aborted" "$stderr_file" 2>/dev/null; then
            fail_reason+="ABORTED "
        fi
        if grep -qi "core dumped" "$stderr_file" 2>/dev/null; then
            fail_reason+="CORE_DUMP "
        fi
        if grep -qi "addresssanitizer" "$stderr_file" 2>/dev/null; then
            fail_reason+="ASAN "
        fi
    fi

    if [[ -n "$fail_reason" ]]; then
        FAILED=$((FAILED + 1))
        FAIL_NAMES+=("$test_name")
        FAIL_REASONS+=("$fail_reason")
        if [[ $exit_code -ne 0 ]] && [[ $exit_code -gt 128 ]]; then
            CRASHED=$((CRASHED + 1))
        fi
        echo "  [FAIL] $test_name — $fail_reason"
    else
        PASSED=$((PASSED + 1))
        echo "  [PASS] $test_name"
    fi
}

# Repeated content generator
repeat_str() {
    local str="$1"
    local count="$2"
    local result=""
    for ((i=0; i<count; i++)); do
        result+="$str"
    done
    printf '%s' "$result"
}

echo "============================================"
echo " txtfmt Stress Testing"
echo "============================================"
echo "Binary: $TXTFMT"
echo "Working directory: $BASE_TEST_DIR"
echo "============================================"
echo ""

# ============================================================
# 1. EDGE CASES
# ============================================================
echo "--- 1. Edge Cases ---"

# 1.1 Empty file
run_single_test "1.1 Empty .txtm file" "" 1

# 1.2 File with only spaces/tabs
run_single_test "1.2 Spaces and tabs only" "  	 	   " 1

# 1.3 Opening tags only
run_single_test "1.3 Opening tags only" "<p><h1>Title<center><frame><list" 1

# 1.4 Closing tags only
run_single_test "1.4 Closing tags only" "</p></h1></center></frame></list>" 1

# 1.5 100 <sep> in a row
run_single_test "1.5 100 consecutive <sep>" "$(repeat_str '<sep>' 100)" 1

# 1.6 <lines 100>
run_single_test "1.6 <lines 100>" "<lines 100>" 1

# 1.7 <doc_width 1> (below minimum)
run_single_test "1.7 <doc_width 1> (below minimum)" "<doc_width 1><sep>" 1

# 1.8 <doc_width 300> (above maximum)
run_single_test "1.8 <doc_width 300> (above maximum)" "<doc_width 300><sep>" 1

# 1.9 Empty <p></p>
run_single_test "1.9 <p></p> empty paragraph" "<p></p>" 1

# 1.10 Empty <frame></frame>
run_single_test "1.10 <frame></frame> empty frame" "<frame></frame>" 1

# 1.11 Empty <list></list>
run_single_test "1.11 <list></list> empty list" "<list></list>" 1

# 1.12 Empty <table></table>
run_single_test "1.12 <table></table> empty table" "<table></table>" 1

# 1.13 Empty <histogram></histogram>
run_single_test "1.13 <histogram></histogram> empty histogram" "<histogram></histogram>" 1

# 1.14 Empty <calc></calc>
run_single_test "1.14 <calc></calc> empty calc" "<calc></calc>" 1

echo ""

# ============================================================
# 2. UNICODE AND SPECIAL CHARACTERS
# ============================================================
echo "--- 2. Unicode and special characters ---"

# 2.1 Cyrillic
run_single_test "2.1 Cyrillic <h1>" "<h1>Привет мир</h1>" 1

# 2.2 CJK
run_single_test "2.2 CJK <p>" "<p>中文测试</p>" 1

# 2.3 Emoji
run_single_test "2.3 Emoji <h1>" "<h1>🔥Test🚀</h1>" 1

# 2.4 Mix UTF-8 + tags
run_single_test "2.4 Mix UTF-8 + tags" "<h1>Привет 世界 🌍</h1>
<p>Cyrillic: Привет</p>
<p>Chinese: 你好</p>
<calc>1+1</calc>
<h3>Emoji: 🎉🎊🎈</h3>" 1

# 2.5 Control characters \x00-\x1f (except \n, \t)
python3 -c "
import sys
data = b''
for c in range(0x00, 0x20):
    if c not in (0x0a, 0x0d, 0x09):  # skip \n, \r, \t
        data += bytes([c])
data += b'<p>Test after control chars</p>\n'
with open('$BASE_TEST_DIR/test_control/input.txtm', 'wb') as f:
    f.write(data)
" 2>/dev/null || mkdir -p "$BASE_TEST_DIR/test_control"
mkdir -p "$BASE_TEST_DIR/test_control"
python3 -c "
import sys
data = b''
for c in range(0x00, 0x20):
    if c not in (0x0a, 0x0d, 0x09):
        data += bytes([c])
data += b'<p>Test after control chars</p>\n'
with open('$BASE_TEST_DIR/test_control/input.txtm', 'wb') as f:
    f.write(data)
"
TOTAL=$((TOTAL + 1))
test_dir="$BASE_TEST_DIR/test_control"
exit_code=0
(cd "$test_dir" && "$TXTFMT" > "$test_dir/stdout.txt" 2>"$test_dir/stderr.txt") || exit_code=$?
fail_reason=""
if [[ $exit_code -ne 0 ]]; then
    fail_reason+="exit_code=$exit_code "
fi
if [[ $exit_code -eq 0 ]] && [[ ! -f "$test_dir/input.txt" ]]; then
    fail_reason+="no_output_txt "
fi
if [[ -f "$test_dir/stderr.txt" ]]; then
    grep -qi "segmentation fault" "$test_dir/stderr.txt" 2>/dev/null && fail_reason+="SEGFAULT "
    grep -qi "aborted" "$test_dir/stderr.txt" 2>/dev/null && fail_reason+="ABORTED "
    grep -qi "core dumped" "$test_dir/stderr.txt" 2>/dev/null && fail_reason+="CORE_DUMP "
fi
if [[ -n "$fail_reason" ]]; then
    FAILED=$((FAILED + 1))
    FAIL_NAMES+=("2.5 Control chars \\x00-\\x1f")
    FAIL_REASONS+=("$fail_reason")
    [[ $exit_code -gt 128 ]] && CRASHED=$((CRASHED + 1))
    echo "  [FAIL] 2.5 Control chars \\x00-\\x1f — $fail_reason"
else
    PASSED=$((PASSED + 1))
    echo "  [PASS] 2.5 Control chars \\x00-\\x1f"
fi

echo ""

# ============================================================
# 3. STRESS TESTS
# ============================================================
echo "--- 3. Stress tests ---"

# 3.1 1000 lines of <calc>1+1</calc>
calc_1000=""
for ((i=0; i<1000; i++)); do
    calc_1000+="<calc>1+1</calc>"$'\n'
done
run_single_test "3.1 1000 lines of <calc>" "$calc_1000" 1

# 3.2 50x50 table
table_50x50="<table>"$'\n'
# Headers
header=""
for ((c=0; c<50; c++)); do
    [[ $c -gt 0 ]] && header+="|"
    header+="Col$c"
done
table_50x50+="$header"$'\n'
# Data rows
for ((r=0; r<49; r++)); do
    row=""
    for ((c=0; c<50; c++)); do
        [[ $c -gt 0 ]] && row+="|"
        row+="$((r * 50 + c))"
    done
    table_50x50+="$row"$'\n'
done
table_50x50+="</table>"
run_single_test "3.2 50x50 table" "$table_50x50" 1

# 3.3 Histogram with 100 values
hist_100="<histogram>"$'\n'
for ((i=0; i<100; i++)); do
    hist_100+="Item_$i|$(( (i * 37 + 13) % 1000 ))"$'\n'
done
hist_100+="</histogram>"
run_single_test "3.3 Histogram with 100 values" "$hist_100" 1

# 3.4 Nesting 20 levels deep
nested_20=""
close_tags=""
for ((i=0; i<20; i++)); do
    nested_20+="<center>"
    close_tags="</center>$close_tags"
done
nested_20+="<calc>1+1</calc>"
nested_20+="$close_tags"
run_single_test "3.4 Nesting 20 levels deep" "$nested_20" 1

# 3.5 ~500KB file
big_content=""
chunk="<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. </p>"
# Calculate how many repetitions needed for ~500KB
target_size=500000
chunk_size=${#chunk}
repeats=$((target_size / chunk_size + 1))
big_content=$(repeat_str "$chunk" $repeats)
run_single_test "3.5 File ~500KB" "$big_content" 1

echo ""

# ============================================================
# 4. SECURITY
# ============================================================
echo "--- 4. Security (errors allowed) ---"

# 4.1 <insert /etc/passwd>
run_single_test "4.1 <insert /etc/passwd>" "<insert /etc/passwd>" 0

# 4.2 <insert ../../etc/passwd>
run_single_test "4.2 <insert ../../etc/passwd>" "<insert ../../etc/passwd>" 0

# 4.3 <insert ../.env>
run_single_test "4.3 <insert ../.env>" "<insert ../.env>" 0

# 4.4 Tag with 10000 spaces
spaces_10k=$(python3 -c "print('<' + ' '*10000 + 'p' + ' '*10000 + '>')")
run_single_test "4.4 Tag with 10000 spaces" "$spaces_10k" 1

echo ""

# ============================================================
# 5. TAG COMBINATIONS
# ============================================================
echo "--- 5. Tag combinations ---"

# 5.1 <center><h1>Title</h1></center>
run_single_test "5.1 <center><h1>Title</h1></center>" "<center><h1>Title</h1></center>" 1

# 5.2 <frame><list *>A\nB\nC</list></frame>
run_single_test "5.2 <frame><list *>A\nB\nC</list></frame>" "<frame><list *>A
B
C</list></frame>" 1

# 5.3 <doc_width 40><table>A|B\n1|2</table>
run_single_test "5.3 <doc_width 40><table>A|B\n1|2</table>" "<doc_width 40><table>A|B
1|2</table>" 1

# 5.4 <right><calc>10*10</calc></right>
run_single_test "5.4 <right><calc>10*10</calc></right>" "<right><calc>10*10</calc></right>" 1

# ============================================================
# ADDITIONAL HARD TESTS
# ============================================================
echo ""
echo "--- 6. Additional hard tests ---"

# 6.1 File with 10000 <sep> tags
run_single_test "6.1 10000 <sep>" "$(repeat_str '<sep>' 10000)" 1

# 6.2 Binary file with 0x00 in the middle
mkdir -p "$BASE_TEST_DIR/test_binary_null"
python3 -c "
data = b'<p>Hello</p>\x00\x00\x00<p>World</p>\n'
with open('$BASE_TEST_DIR/test_binary_null/input.txtm', 'wb') as f:
    f.write(data)
"
TOTAL=$((TOTAL + 1))
test_dir="$BASE_TEST_DIR/test_binary_null"
exit_code=0
(cd "$test_dir" && "$TXTFMT" > "$test_dir/stdout.txt" 2>"$test_dir/stderr.txt") || exit_code=$?
fail_reason=""
if [[ $exit_code -ne 0 ]]; then
    fail_reason+="exit_code=$exit_code "
fi
if [[ $exit_code -eq 0 ]] && [[ ! -f "$test_dir/input.txt" ]]; then
    fail_reason+="no_output_txt "
fi
if [[ -f "$test_dir/stderr.txt" ]]; then
    grep -qi "segmentation fault" "$test_dir/stderr.txt" 2>/dev/null && fail_reason+="SEGFAULT "
    grep -qi "aborted" "$test_dir/stderr.txt" 2>/dev/null && fail_reason+="ABORTED "
    grep -qi "core dumped" "$test_dir/stderr.txt" 2>/dev/null && fail_reason+="CORE_DUMP "
fi
if [[ -n "$fail_reason" ]]; then
    FAILED=$((FAILED + 1))
    FAIL_NAMES+=("6.2 Binary with null bytes")
    FAIL_REASONS+=("$fail_reason")
    [[ $exit_code -gt 128 ]] && CRASHED=$((CRASHED + 1))
    echo "  [FAIL] 6.2 Binary with null bytes — $fail_reason"
else
    PASSED=$((PASSED + 1))
    echo "  [PASS] 6.2 Binary with null bytes"
fi

# 6.3 Very long line without tags (100K chars)
long_line=$(python3 -c "print('A' * 100000)")
run_single_test "6.3 100K char line" "$long_line" 1

# 6.4 Deep nesting 100 levels
nested_100=""
close_tags=""
for ((i=0; i<100; i++)); do
    nested_100+="<center>"
    close_tags="</center>$close_tags"
done
nested_100+="Deep Text"
nested_100+="$close_tags"
run_single_test "6.4 Nesting 100 levels deep" "$nested_100" 1

# 6.5 500 nested calc
nested_calc=""
close_tags=""
for ((i=0; i<500; i++)); do
    nested_calc+="<calc>"
    close_tags="</calc>$close_tags"
done
nested_calc+="42"
nested_calc+="$close_tags"
run_single_test "6.5 500 nested <calc>" "$nested_calc" 1

# 6.6 File with 5000 lines
run_single_test "6.6 <lines 5000>" "<lines 5000>" 1

# 6.7 Mix of all tags 500 times
mixed_500=""
for ((i=0; i<500; i++)); do
    mixed_500+="<p>Paragraph $i</p>
<h2>Header $i</h2>
<sep>
<calc>$i+$i</calc>
"
done
run_single_test "6.7 Mix of all tags 500x" "$mixed_500" 1

# 6.8 UnicodeBomb — many emoji
emoji_bomb=$(python3 -c "print('🔥' * 5000)")
run_single_test "6.8 Emoji bomb (5000 🔥)" "<p>$emoji_bomb</p>" 1

# 6.9 <insert> on nonexistent file
run_single_test "6.9 <insert /nonexistent/file>" "<insert /nonexistent/file.txt>" 0

# 6.10 File of only opening angle brackets
run_single_test "6.10 File of '<' (1000)" "$(repeat_str '<' 1000)" 1

# 6.11 File of only closing angle brackets
run_single_test "6.11 File of '>' (1000)" "$(repeat_str '>' 1000)" 1

# 6.12 Many <doc_width> tags in a row
doc_width_many=""
for ((w=10; w<=250; w+=10)); do
    doc_width_many+="<doc_width $w><sep>
"
done
run_single_test "6.12 <doc_width> 10 to 250" "$doc_width_many" 1

# 6.13 ~1MB file
big_1mb=""
chunk="<p>The quick brown fox jumps over the lazy dog. </p>"
chunk_size=${#chunk}
repeats_1mb=$((1000000 / chunk_size + 1))
big_1mb=$(repeat_str "$chunk" $repeats_1mb)
run_single_test "6.13 File ~1MB" "$big_1mb" 1

# 6.14 Wrong order of closing tags
run_single_test "6.14 Wrong close order" "<p><center>Text</p></center>" 1

# 6.15 File with BOM (UTF-8 BOM: EF BB BF)
mkdir -p "$BASE_TEST_DIR/test_bom"
python3 -c "
data = b'\xef\xbb\xbf<p>BOM test</p>\n'
with open('$BASE_TEST_DIR/test_bom/input.txtm', 'wb') as f:
    f.write(data)
"
TOTAL=$((TOTAL + 1))
test_dir="$BASE_TEST_DIR/test_bom"
exit_code=0
(cd "$test_dir" && "$TXTFMT" > "$test_dir/stdout.txt" 2>"$test_dir/stderr.txt") || exit_code=$?
fail_reason=""
if [[ $exit_code -ne 0 ]]; then
    fail_reason+="exit_code=$exit_code "
fi
if [[ $exit_code -eq 0 ]] && [[ ! -f "$test_dir/input.txt" ]]; then
    fail_reason+="no_output_txt "
fi
if [[ -f "$test_dir/stderr.txt" ]]; then
    grep -qi "segmentation fault" "$test_dir/stderr.txt" 2>/dev/null && fail_reason+="SEGFAULT "
    grep -qi "aborted" "$test_dir/stderr.txt" 2>/dev/null && fail_reason+="ABORTED "
    grep -qi "core dumped" "$test_dir/stderr.txt" 2>/dev/null && fail_reason+="CORE_DUMP "
fi
if [[ -n "$fail_reason" ]]; then
    FAILED=$((FAILED + 1))
    FAIL_NAMES+=("6.15 UTF-8 BOM")
    FAIL_REASONS+=("$fail_reason")
    [[ $exit_code -gt 128 ]] && CRASHED=$((CRASHED + 1))
    echo "  [FAIL] 6.15 UTF-8 BOM — $fail_reason"
else
    PASSED=$((PASSED + 1))
    echo "  [PASS] 6.15 UTF-8 BOM"
fi

# ============================================================
# SUMMARY TABLE
# ============================================================
echo ""
echo "============================================"
echo " RESULTS SUMMARY"
echo "============================================"
echo ""
printf "%-55s | %s\n" "TEST" "RESULT"
printf "%-55s | %s\n" "-------------------------------------------------------" "--------"

# List all tests by name
test_idx=0
all_tests=(
    "1.1 Empty .txtm file"
    "1.2 Spaces and tabs only"
    "1.3 Opening tags only"
    "1.4 Closing tags only"
    "1.5 100 consecutive <sep>"
    "1.6 <lines 100>"
    "1.7 <doc_width 1> (below minimum)"
    "1.8 <doc_width 300> (above maximum)"
    "1.9 <p></p> empty paragraph"
    "1.10 <frame></frame> empty frame"
    "1.11 <list></list> empty list"
    "1.12 <table></table> empty table"
    "1.13 <histogram></histogram> empty histogram"
    "1.14 <calc></calc> empty calc"
    "2.1 Cyrillic <h1>"
    "2.2 CJK <p>"
    "2.3 Emoji <h1>"
    "2.4 Mix UTF-8 + tags"
    "2.5 Control chars \\x00-\\x1f"
    "3.1 1000 lines of <calc>"
    "3.2 50x50 table"
    "3.3 Histogram with 100 values"
    "3.4 Nesting 20 levels deep"
    "3.5 File ~500KB"
    "4.1 <insert /etc/passwd>"
    "4.2 <insert ../../etc/passwd>"
    "4.3 <insert ../.env>"
    "4.4 Tag with 10000 spaces"
    "5.1 <center><h1>Title</h1></center>"
    "5.2 <frame><list *>A\\nB\\nC</list></frame>"
    "5.3 <doc_width 40><table>A|B\\n1|2</table>"
    "5.4 <right><calc>10*10</calc></right>"
    "6.1 10000 <sep>"
    "6.2 Binary with null bytes"
    "6.3 100K char line"
    "6.4 Nesting 100 levels deep"
    "6.5 500 nested <calc>"
    "6.6 <lines 5000>"
    "6.7 Mix of all tags 500x"
    "6.8 Emoji bomb (5000)"
    "6.9 <insert /nonexistent/file>"
    "6.10 File of '<' (1000)"
    "6.11 File of '>' (1000)"
    "6.12 <doc_width> 10 to 250"
    "6.13 File ~1MB"
    "6.14 Wrong close order"
    "6.15 UTF-8 BOM"
)

# Build set of failed tests for fast lookup
declare -A failed_set
for ((i=0; i<${#FAIL_NAMES[@]}; i++)); do
    failed_set["${FAIL_NAMES[$i]}"]="${FAIL_REASONS[$i]}"
done

for tname in "${all_tests[@]}"; do
    if [[ -n "${failed_set[$tname]+x}" ]]; then
        printf "%-55s | %s\n" "$tname" "FAIL (${failed_set[$tname]})"
    else
        printf "%-55s | %s\n" "$tname" "PASS"
    fi
done

echo ""
echo "============================================"
echo " SUMMARY"
echo "============================================"
echo "Total tests:     $TOTAL"
echo "Passed:          $PASSED"
echo "Failed:          $FAILED"
echo "  crashed:       $CRASHED"
echo "============================================"

if [[ $FAILED -gt 0 ]]; then
    echo ""
    echo "FAILED TESTS:"
    for ((i=0; i<${#FAIL_NAMES[@]}; i++)); do
        echo "  [$((i+1))] ${FAIL_NAMES[$i]} — ${FAIL_REASONS[$i]}"
    done
    echo ""
    echo "Test files: $BASE_TEST_DIR"
    exit 1
else
    echo ""
    echo "ALL TESTS PASSED!"
    exit 0
fi
