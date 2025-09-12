#!/bin/sh
set -eu
cd "$(dirname "$0")"
export LD_LIBRARY_PATH="$(pwd)/lib:${LD_LIBRARY_PATH:-}"

INTRO_VIDEO=./videos/Intro.mp4
FFPLAY=./bin/ffplay
INTRO_MAX_SECONDS=${PLUNDER_INTRO_MAX_SECONDS:-10}
INTRO_ONCE_FLAG=/tmp/plunder_intro_once

# Skip conditions
if { [ -f ./config.json ] && grep -q '"disable_intro"[[:space:]]*:[[:space:]]*true' ./config.json; } || \
   [ "${PLUNDER_SKIP_INTRO:-0}" = 1 ] || [ -f /tmp/plunder_skip_intro ]; then
  exec ./Plunder
fi

# Only play once per boot if file present
if [ -f ./intro_once ] && [ -f "$INTRO_ONCE_FLAG" ]; then
  exec ./Plunder
fi

play_intro() {
  SMOOTH=${PLUNDER_INTRO_SMOOTH:-1}
  if [ "$SMOOTH" = 1 ]; then
    ANALYSE="-analyzeduration 3000000 -probesize 3000000 -sync video -noframedrop"
  else
    ANALYSE="-analyzeduration 500000 -probesize 500000"
  fi
  # Auto threads (cap 3)
  CORES=$( (command -v nproc >/dev/null && nproc) || grep -c '^processor' /proc/cpuinfo 2>/dev/null || echo 1 )
  [ "$CORES" -gt 3 ] && CORES=3
  SRC=$INTRO_VIDEO
  if [ "${PLUNDER_INTRO_TMPFS:-0}" = 1 ]; then
    for T in /dev/shm /tmp; do
      if cp -f "$INTRO_VIDEO" "$T/plunder_intro.mp4" 2>/dev/null; then SRC="$T/plunder_intro.mp4"; break; fi
    done
  fi
  "$FFPLAY" -autoexit -nostats -hide_banner -loglevel warning -threads "$CORES" $ANALYSE "$SRC" 2>/dev/null &
  PID=$!
  START=$(date +%s)
  while kill -0 $PID 2>/dev/null; do
    ELAP=$(( $(date +%s) - START ))
    if [ $ELAP -ge $INTRO_MAX_SECONDS ]; then
      kill -INT $PID 2>/dev/null || true; sleep 1
      kill -TERM $PID 2>/dev/null || true; sleep 1
      kill -KILL $PID 2>/dev/null || true; break
    fi
    sleep 0.2
  done
}

if [ -x "$FFPLAY" ] && [ -f "$INTRO_VIDEO" ]; then
  play_intro
  [ -f ./intro_once ] && : > "$INTRO_ONCE_FLAG"
  sleep 0.2
fi

exec ./Plunder