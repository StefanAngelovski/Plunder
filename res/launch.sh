#!/bin/sh
set -eu
cd "$(dirname "$0")"
export LD_LIBRARY_PATH="$(pwd)/lib:${LD_LIBRARY_PATH:-}"

# ==================== AUTO-UPDATE HANDLING ====================
UPDATE_MARKER="./.update_pending"
UPDATE_STAGING="./.update_staging"

apply_update() {
    if [ ! -f "$UPDATE_MARKER" ]; then
        return 0
    fi
    
    echo "[Plunder] Pending update detected, applying..."
    
    # Read the zip path from marker file
    ZIP_PATH=$(grep '^ZIP_PATH=' "$UPDATE_MARKER" | cut -d'=' -f2-)
    VERSION=$(grep '^VERSION=' "$UPDATE_MARKER" | cut -d'=' -f2-)
    
    if [ -z "$ZIP_PATH" ] || [ ! -f "$ZIP_PATH" ]; then
        echo "[Plunder] Error: Update zip not found at $ZIP_PATH"
        rm -f "$UPDATE_MARKER"
        return 1
    fi
    
    echo "[Plunder] Extracting update $VERSION from $ZIP_PATH"
    
    # Create backup of current binary in case update fails
    if [ -f ./Plunder ]; then
        cp ./Plunder ./Plunder.bak 2>/dev/null || true
    fi
    
    # Extract the update (overwrite existing files)
    # The zip should contain a Plunder folder at root
    TEMP_EXTRACT="/tmp/plunder_update_extract"
    rm -rf "$TEMP_EXTRACT"
    mkdir -p "$TEMP_EXTRACT"
    
    if unzip -o "$ZIP_PATH" -d "$TEMP_EXTRACT" >/dev/null 2>&1; then
        # Check if extracted to Plunder subfolder or directly
        if [ -d "$TEMP_EXTRACT/Plunder" ]; then
            cp -rf "$TEMP_EXTRACT/Plunder/"* ./ 2>/dev/null || true
        else
            cp -rf "$TEMP_EXTRACT/"* ./ 2>/dev/null || true
        fi
        
        # Make sure the new binary is executable
        chmod +x ./Plunder 2>/dev/null || true
        chmod +x ./launch.sh 2>/dev/null || true
        chmod +x ./bin/* 2>/dev/null || true
        
        echo "[Plunder] Update applied successfully to version $VERSION"
        
        # Clean up
        rm -rf "$TEMP_EXTRACT"
        rm -rf "$UPDATE_STAGING"
        rm -f "$UPDATE_MARKER"
        rm -f ./Plunder.bak
        
        return 0
    else
        echo "[Plunder] Error: Failed to extract update"
        # Restore backup if extraction failed
        if [ -f ./Plunder.bak ]; then
            mv ./Plunder.bak ./Plunder
        fi
        rm -rf "$TEMP_EXTRACT"
        rm -f "$UPDATE_MARKER"
        return 1
    fi
}

# Apply any pending update before starting
apply_update

# ==================== INTRO VIDEO HANDLING ====================
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