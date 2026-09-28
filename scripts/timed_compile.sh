#!/usr/bin/env bash
# Compiler wrapper to measure translation unit build time

LOG_FILE="${HESIOD_BUILD_TIME_LOG:-build/.hesiod_build_times.log}"

# Find source file name from arguments
SOURCE_FILE=""
for arg in "$@"; do
    if [[ "$arg" == *.cpp ]] || [[ "$arg" == *.cc ]] || [[ "$arg" == *.cxx ]]; then
        SOURCE_FILE="$arg"
        break
    fi
done

START_TIME=$(date +%s%N)
"$@"
STATUS=$?
END_TIME=$(date +%s%N)

if [ -n "$SOURCE_FILE" ] && [ -n "$LOG_FILE" ]; then
    DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
    LOG_DIR="$(dirname "${LOG_FILE}")"
    if [ -d "$LOG_DIR" ]; then
        echo -e "${DURATION_MS}\t${SOURCE_FILE}" >> "${LOG_FILE}" 2>/dev/null || true
    fi
fi

exit $STATUS
