#!/usr/bin/env bash

source .env

SCOREBASEURL="https://gameinfo.daraempire.com/api/v001/khor-services/GameTracker/SendScore"

PerfSendScore() {
    local CHARNAME="$1"
    local SCORE="$2"

    curl -sS -o /dev/null \
        -w "%{http_code}|%{time_total}|${CHARNAME}|${SCORE}\n" \
        -X POST \
        -H "Content-Type: application/json" \
        -H "X-Api-Key: $APIKEY" \
        -d "{\"score\":$SCORE,\"avatarId\":\"A1\",\"charName\":\"$CHARNAME\"}" \
        "$SCOREBASEURL"
}

PerfTestParallel() {
    local TOTAL_REQUESTS="${1:-500}"
    local PARALLEL_JOBS="${2:-50}"
    local SCORE_MIN="${3:-1}"
    local SCORE_MAX="${4:-200}"

    local TMPFILE
    TMPFILE=$(mktemp)

    echo "----------------------------------------"
    echo "GameTracker SendScore performance test"
    echo "Total requests : $TOTAL_REQUESTS"
    echo "Parallel jobs  : $PARALLEL_JOBS"
    echo "Score range    : $SCORE_MIN - $SCORE_MAX"
    echo "----------------------------------------"

    local START_MS
    START_MS=$(date +%s%3N)

    export APIKEY SCOREBASEURL
    export -f PerfSendScore

    seq 1 "$TOTAL_REQUESTS" | xargs -I{} -P "$PARALLEL_JOBS" bash -c '
        N="$1"
        NAME="PerfUser_$N"
        SCORE=$(( RANDOM % ('"$SCORE_MAX"' - '"$SCORE_MIN"' + 1) + '"$SCORE_MIN"' ))
        PerfSendScore "$NAME" "$SCORE"
    ' _ {} > "$TMPFILE"

    local END_MS
    END_MS=$(date +%s%3N)
    local DURATION_MS=$((END_MS - START_MS))

    local SUCCESS_COUNT
    local FAIL_COUNT
    local AVG_REQ_TIME
    local RPS

    SUCCESS_COUNT=$(awk -F'|' '$1 ~ /^2[0-9][0-9]$/ {c++} END {print c+0}' "$TMPFILE")
    FAIL_COUNT=$(awk -F'|' '$1 !~ /^2[0-9][0-9]$/ {c++} END {print c+0}' "$TMPFILE")
    AVG_REQ_TIME=$(awk -F'|' '{sum+=$2; n++} END {if(n>0) printf "%.4f", sum/n; else print "0"}' "$TMPFILE")
    RPS=$(awk -v total="$TOTAL_REQUESTS" -v dur="$DURATION_MS" 'BEGIN {if(dur>0) printf "%.2f", total/(dur/1000); else print "0"}')

    echo
    echo "Results:"
    echo "  Total duration      : ${DURATION_MS} ms"
    echo "  Successful requests : $SUCCESS_COUNT"
    echo "  Failed requests     : $FAIL_COUNT"
    echo "  Avg req time        : ${AVG_REQ_TIME} s"
    echo "  Approx requests/sec : $RPS"

    echo
    echo "HTTP status distribution:"
    awk -F'|' '{count[$1]++} END {for (code in count) print "  " code ": " count[code]}' "$TMPFILE" | sort

    echo
    echo "Top 10 slowest requests:"
    sort -t'|' -k2 -nr "$TMPFILE" | head -10 | awk -F'|' '{printf "  HTTP %s | %ss | %s | score=%s\n", $1, $2, $3, $4}'

    rm -f "$TMPFILE"
}

# Example:
PerfTestParallel 5000 200