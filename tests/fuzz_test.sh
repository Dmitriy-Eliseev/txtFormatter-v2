#!/bin/bash
#
# txtfmt fuzz testing
# Generates random .txtm files and checks program stability
#

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TXTFMT="$SCRIPT_DIR/../txtfmt"
FUZZ_DIR="/tmp/txtfmt_fuzz_$$"
NUM_TESTS=150
PASSED=0
FAILED=0
CRASHED=0
ASAN_ERRORS=0
NO_OUTPUT=0

# Patterns for generation
TAGS=("<p>" "<h1>" "<h2>" "<h3>" "<h4>" "<table>" "<calc>" "<histogram>"
      "<list>" "<frame>" "<sep>" "<center>" "<right>" "<lines>" "<insert>"
      "<doc_width>" "<default_width>" "<date>" "<time>" "<datetime>")
CLOSE_TAGS=("</p>" "</h1>" "</h2>" "</h3>" "</h4>" "</table>" "</calc>"
            "</histogram>" "</list>" "</frame>" "</center>" "</right>"
            "</lines>" "</insert>" "</doc_width>" "</default_width>")
INVALID_TAGS=("<invalid>" "<p 123>" "<>" "<123>" "<tag with spaces>"
              "<unclosed" "<<" ">>" "</>" "<p>" "<h5>" "<tablex>"
              "<calc foo=bar>" "<>" "<>" "</nonexistent>")
UNICODE_SAMPLES=("hello world" "你好世界" "こんにちは" "🎉🚀💻" "Greek ΔΣ"
                 "αβγδε" "日本語テスト" "Cyrillic Привет" "Mixed: Hello мир 世界 🌍")

mkdir -p "$FUZZ_DIR"

echo "============================================"
echo " txtfmt Fuzz Testing"
echo "============================================"
echo "Binary: $TXTFMT"
echo "Number of tests: $NUM_TESTS"
echo "Working directory: $FUZZ_DIR"
echo "============================================"
echo ""

# Generate random printable string
random_printable() {
    local len=${1:-20}
    cat /dev/urandom | tr -dc 'a-zA-Z0-9 .,!?;:-_\n\t' | head -c "$len"
}

# Generate string with control characters \x00-\x1f
random_control_chars() {
    local len=${1:-10}
    python3 -c "
import random, sys
chars = ''.join(chr(random.randint(0, 31)) for _ in range($len))
sys.stdout.write(chars)
" 2>/dev/null
}

# Generator 1: Random bytes including control chars
gen_random_bytes() {
    local out_file="$1"
    python3 -c "
import random, sys
size = random.randint(50, 2000)
data = bytearray()
for _ in range(size):
    # Mix of printable, control chars, and high bytes
    r = random.random()
    if r < 0.6:
        data.append(random.randint(32, 126))
    elif r < 0.8:
        data.append(random.randint(0, 31))
    else:
        data.append(random.randint(127, 255))
with open('$out_file', 'wb') as f:
    f.write(data)
" 2>/dev/null
    echo "random_bytes"
}

# Generator 2: Random tags with content
gen_random_tags() {
    local out_file="$1"
    local num_tags=$((RANDOM % 30 + 5))
    local content=""

    for ((i=0; i<num_tags; i++)); do
        local tag_idx=$((RANDOM % ${#TAGS[@]}))
        local tag="${TAGS[$tag_idx]}"
        local tag_text="${tag//<\(\/?\)/}"
        tag_text="${tag_text//[<>]/}"

        # Random content
        local r=$((RANDOM % 4))
        if [[ $r -eq 0 ]]; then
            content+="$tag$(random_printable $((RANDOM % 100 + 5)))"
        elif [[ $r -eq 1 ]]; then
            content+="$tag$(random_printable $((RANDOM % 50 + 1)))"
        else
            # Empty tag
            content+="$tag"
        fi
        content+=$'\n'
    done

    echo "$content" > "$out_file"
    echo "random_tags"
}

# Generator 3: Deep tag nesting
gen_deep_nesting() {
    local out_file="$1"
    local depth=$((RANDOM % 50 + 10))
    local content=""

    # Opening tags
    local open_tags=()
    for ((i=0; i<depth; i++)); do
        local tag_idx=$((RANDOM % ${#TAGS[@]}))
        local tag="${TAGS[$tag_idx]}"
        content+="$tag"
        open_tags+=("$tag")
        if (( i % 5 == 0 )); then
            content+=$'\n'
        fi
    done

    # Content at depth
    content+=$'\n'"Deep content $(random_printable 20)"$'\n'

    # Closing tags (maybe not all)
    local close_depth=$((RANDOM % (depth + 1)))
    if [[ $close_depth -lt ${#open_tags[@]} ]]; then
        # Unclosed tags
        for ((i=0; i<close_depth; i++)); do
            local idx=$((depth - 1 - i))
            local tag="${open_tags[$idx]}"
            local close_tag="</${tag#<}"
            close_tag="${close_tag/>/}"
            close_tag="$close_tag>"
            content+="$close_tag"
            if (( i % 5 == 0 )); then
                content+=$'\n'
            fi
        done
    fi

    echo "$content" > "$out_file"
    echo "deep_nesting_depth=${depth}_closed=${close_depth}"
}

# Generator 4: Unclosed tags
gen_unclosed_tags() {
    local out_file="$1"
    local content=""
    local num_tags=$((RANDOM % 15 + 3))

    for ((i=0; i<num_tags; i++)); do
        local tag_idx=$((RANDOM % ${#TAGS[@]}))
        content+="${TAGS[$tag_idx]}"
        content+=$(random_printable $((RANDOM % 50 + 5)))
        content+=$'\n'
        # Sometimes add closing tag, sometimes not
        if (( RANDOM % 3 != 0 )); then
            local tag="${TAGS[$tag_idx]}"
            local tag_name="${tag#<}"
            tag_name="${tag_name%% *}"
            content+="</${tag_name}>"
        fi
        content+=$'\n'
    done

    echo "$content" > "$out_file"
    echo "unclosed_tags"
}

# Generator 5: Invalid tags
gen_invalid_tags() {
    local out_file="$1"
    local content=""
    local num_invalid=$((RANDOM % 20 + 5))

    for ((i=0; i<num_invalid; i++)); do
        local idx=$((RANDOM % ${#INVALID_TAGS[@]}))
        content+="${INVALID_TAGS[$idx]}"
        content+=$(random_printable $((RANDOM % 30 + 1)))
        content+=$'\n'
    done

    # Add some valid for contrast
    content+=$'\n'"<p>Valid paragraph here</p>"$'\n'
    content+="<center>Centered text</center>"$'\n'

    echo "$content" > "$out_file"
    echo "invalid_tags"
}

# Generator 6: Unicode characters
gen_unicode() {
    local out_file="$1"
    local content=""
    local num_lines=$((RANDOM % 20 + 5))

    for ((i=0; i<num_lines; i++)); do
        local uidx=$((RANDOM % ${#UNICODE_SAMPLES[@]}))
        content+="${UNICODE_SAMPLES[$uidx]}"
        content+=" $(random_printable $((RANDOM % 30 + 1)))"
        content+=$'\n'
    done

    # Mixed tags with unicode
    local tag_idx=$((RANDOM % ${#TAGS[@]}))
    local uidx=$((RANDOM % ${#UNICODE_SAMPLES[@]}))
    local tag_name="${TAGS[$tag_idx]#<}"
    content+="${TAGS[$tag_idx]}${UNICODE_SAMPLES[$uidx]}"
    content+="</${tag_name}>"$'\n'

    echo "$content" > "$out_file"
    echo "unicode"
}

# Generator 7: Very long lines
gen_long_lines() {
    local out_file="$1"
    local content=""
    local num_lines=$((RANDOM % 5 + 1))

    for ((i=0; i<num_lines; i++)); do
        local line_len=$((RANDOM % 3000 + 1000))
        content+=$(random_printable $line_len)
        content+=$'\n'
    done

    # Add tags with long content
    local tag_idx=$((RANDOM % ${#TAGS[@]}))
    local line_len=$((RANDOM % 2000 + 500))
    content+="${TAGS[$tag_idx]}"
    content+=$(random_printable $line_len)
    local tag_name="${TAGS[$tag_idx]#<}"
    content+="</${tag_name}>"$'\n'

    echo "$content" > "$out_file"
    echo "long_lines"
}

# Generator 8: Many empty lines
gen_empty_lines() {
    local out_file="$1"
    {
        for ((i=0; i<50; i++)); do
            echo ""
        done
        echo "<p>Text after empty lines</p>"
        for ((i=0; i<50; i++)); do
            echo ""
        done
        echo "<h1>More text</h1>"
        for ((i=0; i<30; i++)); do
            echo ""
        done
    } > "$out_file"
    echo "empty_lines"
}

# Generator 9: Mixed (all patterns together)
gen_mixed() {
    local out_file="$1"
    local content=""

    # Random bytes
    content+=$(random_printable $((RANDOM % 200 + 50)))
    content+=$'\n'

    # Tags
    local num_tags=$((RANDOM % 10 + 2))
    for ((i=0; i<num_tags; i++)); do
        local tag_idx=$((RANDOM % ${#TAGS[@]}))
        content+="${TAGS[$tag_idx]}"
        content+=$(random_printable $((RANDOM % 30 + 1)))
        local tag_name="${TAGS[$tag_idx]#<}"
        if (( RANDOM % 2 == 0 )); then
            content+="</${tag_name}>"
        fi
        content+=$'\n'
    done

    # Unicode
    local uidx=$((RANDOM % ${#UNICODE_SAMPLES[@]}))
    content+="${UNICODE_SAMPLES[$uidx]}"$'\n'

    # Invalid
    local idx=$((RANDOM % ${#INVALID_TAGS[@]}))
    content+="${INVALID_TAGS[$idx]}"$'\n'

    # Control chars (via python)
    content+=$(python3 -c "print(''.join(chr(i) for i in range(0, 32)))" 2>/dev/null)
    content+=$'\n'

    echo "$content" > "$out_file"
    echo "mixed"
}

# Generator 10: Tag after tag without content
gen_tag_soup() {
    local out_file="$1"
    local content=""
    local num_tags=$((RANDOM % 100 + 50))

    for ((i=0; i<num_tags; i++)); do
        local r=$((RANDOM % 5))
        if [[ $r -eq 0 ]]; then
            # Opening tag
            local tag_idx=$((RANDOM % ${#TAGS[@]}))
            content+="${TAGS[$tag_idx]}"
        elif [[ $r -eq 1 ]]; then
            # Closing tag (may be unpaired)
            local tag_idx=$((RANDOM % ${#TAGS[@]}))
            local tag_name="${TAGS[$tag_idx]#<}"
            content+="</${tag_name}>"
        elif [[ $r -eq 2 ]]; then
            # Invalid tag
            local idx=$((RANDOM % ${#INVALID_TAGS[@]}))
            content+="${INVALID_TAGS[$idx]}"
        elif [[ $r -eq 3 ]]; then
            content+=$'\n'
        else
            content+=$(random_printable $((RANDOM % 10 + 1)))
        fi
    done

    echo "$content" > "$out_file"
    echo "tag_soup"
}

# Generator array
GENERATORS=(
    gen_random_bytes
    gen_random_tags
    gen_deep_nesting
    gen_unclosed_tags
    gen_invalid_tags
    gen_unicode
    gen_long_lines
    gen_empty_lines
    gen_mixed
    gen_tag_soup
)

# Track patterns that caused crashes
declare -A CRASH_PATTERNS

echo "Starting fuzz testing..."
echo ""

for ((i=1; i<=NUM_TESTS; i++)); do
    TEST_DIR="$FUZZ_DIR/test_$i"
    mkdir -p "$TEST_DIR"

    INPUT_FILE="$TEST_DIR/input.txtm"
    OUTPUT_FILE="$TEST_DIR/output.txt"

    # Choose random generator
    gen_idx=$((RANDOM % ${#GENERATORS[@]}))
    gen_func="${GENERATORS[$gen_idx]}"

    # Generate file and get pattern description
    pattern=$($gen_func "$INPUT_FILE")

    # Run txtfmt
    stderr_file="$TEST_DIR/stderr.txt"
    exit_code=0
    $TXTFMT "$INPUT_FILE" "$OUTPUT_FILE" 2>"$stderr_file"
    exit_code=$?

    # Check result
    test_passed=true
    failure_reason=""

    # 1. Check exit code
    if [[ $exit_code -ne 0 ]]; then
        test_passed=false
        failure_reason+="exit_code=$exit_code "
        ((CRASHED++))

        # Record pattern as crash-causing
        if [[ -n "${CRASH_PATTERNS[$pattern]+x}" ]]; then
            CRASH_PATTERNS[$pattern]=$(( ${CRASH_PATTERNS[$pattern]} + 1 ))
        else
            CRASH_PATTERNS[$pattern]=1
        fi
    fi

    # 2. Check ASan/UBSan errors in stderr
    if [[ -f "$stderr_file" ]]; then
        stderr_content=$(cat "$stderr_file" 2>/dev/null || true)

        if echo "$stderr_content" | grep -q "ERROR:"; then
            test_passed=false
            failure_reason+="ASAN_ERROR "
            ((ASAN_ERRORS++))
        fi

        if echo "$stderr_content" | grep -q "runtime error"; then
            test_passed=false
            failure_reason+="UBSAN_ERROR "
        fi

        if echo "$stderr_content" | grep -q "heap-use-after-free"; then
            test_passed=false
            failure_reason+="HEAP_UAF "
        fi

        if echo "$stderr_content" | grep -q "stack-buffer-overflow"; then
            test_passed=false
            failure_reason+="STACK_OVERFLOW "
        fi
    fi

    # 3. Check output file exists (if exit_code was 0)
    if [[ $exit_code -eq 0 ]] && [[ ! -f "$OUTPUT_FILE" ]]; then
        test_passed=false
        failure_reason+="no_output_file "
        ((NO_OUTPUT++))
    fi

    # Update stats
    if $test_passed; then
        ((PASSED++))
    else
        ((FAILED++))
        # Print failure info (first 10)
        if [[ $FAILED -le 10 ]]; then
            echo "[FAIL] Test #$i | Pattern: $pattern | Reason: $failure_reason"
            if [[ -f "$stderr_file" ]]; then
                # Show first line of error
                first_error=$(head -1 "$stderr_file" 2>/dev/null || true)
                if [[ -n "$first_error" ]]; then
                    echo "       Error: $first_error"
                fi
            fi
        fi
    fi

    # Progress every 25 tests
    if (( i % 25 == 0 )); then
        echo "[PROGRESS] Completed $i/$NUM_TESTS tests..."
    fi
done

echo ""
echo "============================================"
echo " FUZZ TESTING RESULTS"
echo "============================================"
echo "Total tests:         $NUM_TESTS"
echo "Passed:              $PASSED"
echo "Failed/errors:       $FAILED"
echo "  - Crashes:         $CRASHED"
echo "  - ASan errors:     $ASAN_ERRORS"
echo "  - No output:       $NO_OUTPUT"
echo ""

if [[ ${#CRASH_PATTERNS[@]} -gt 0 ]]; then
    echo "CRASH-CAUSING PATTERNS:"
    echo "--------------------------------------------"
    for pattern in "${!CRASH_PATTERNS[@]}"; do
        count=${CRASH_PATTERNS[$pattern]}
        echo "  [$count time(s)] $pattern"
    done
    echo ""
fi

# Show examples of crashing files
if [[ $CRASHED -gt 0 ]]; then
    echo "CRASHING FILE EXAMPLES:"
    echo "--------------------------------------------"
    crash_count=0
    for ((i=1; i<=NUM_TESTS && crash_count<3; i++)); do
        TEST_DIR="$FUZZ_DIR/test_$i"
        stderr_file="$TEST_DIR/stderr.txt"
        if [[ -f "$stderr_file" ]] && grep -q "ERROR:" "$stderr_file" 2>/dev/null; then
            ((crash_count++))
            echo ""
            echo "--- Test #$i ---"
            echo "Input (hex):"
            xxd "$TEST_DIR/input.txtm" 2>/dev/null | head -10
            echo ""
            echo "Error:"
            head -3 "$stderr_file"
        fi
    done
    echo ""
fi

echo "Working files saved to: $FUZZ_DIR"
echo "============================================"

# Don't delete directory so it can be analyzed
echo ""
echo "To analyze crashing files:"
echo "  ls $FUZZ_DIR/test_*/stderr.txt | head -5"
echo "  xxd $FUZZ_DIR/test_N/input.txtm | head -20"
