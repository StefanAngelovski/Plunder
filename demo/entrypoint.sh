#!/usr/bin/env bash
# Plunder demo supervisor:
#   - virtual display (Xvfb) -> x11vnc -> websockify/noVNC on :6080
#   - builds the app from the read-only repo mount at /src
#   - polls the sources; on change, rebuilds and (only if the build succeeds)
#     restarts the app with the new binary
#   - restarts the app if it exits (e.g. "Exit" chosen in the menu)
set -u

SRC=/src
WORK=/work
APP=/mnt/SDCARD/Apps/Plunder          # same path the app hardcodes on the device
WEB=/web
LOGS=$WEB/logs
STATUS=$WEB/status.json
POLL_SECONDS=${PLUNDER_DEMO_POLL:-2}

mkdir -p "$WORK" "$APP" "$LOGS"
export DISPLAY=:0 SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=x11

BUILD_NO=0
BUILD_STATE=starting
BUILD_TIME=
BUILD_COMMIT=
APP_PID=

write_status() {
    cat > "$STATUS.tmp" <<EOF
{"state":"$BUILD_STATE","build":$BUILD_NO,"time":"$BUILD_TIME","commit":"$BUILD_COMMIT","app_running":$( [ -n "$APP_PID" ] && kill -0 "$APP_PID" 2>/dev/null && echo true || echo false )}
EOF
    mv "$STATUS.tmp" "$STATUS"
}

fingerprint() {
    find "$SRC/src" "$SRC/res" "$SRC/Makefile" -type f -printf '%T@ %s %p\n' 2>/dev/null | sort | md5sum | cut -d' ' -f1
}

commit_label() {
    local c
    c=$(git -C "$SRC" rev-parse --short HEAD 2>/dev/null || echo unknown)
    git -C "$SRC" diff --quiet HEAD -- src res Makefile 2>/dev/null || c="$c+dirty"
    echo "$c"
}

build() {
    BUILD_NO=$((BUILD_NO + 1))
    BUILD_STATE=building; write_status
    echo "[demo] build #$BUILD_NO starting"
    # Copy changed sources, then mark them as just-modified: rsync keeps the
    # original timestamps, and a file saved while the previous build was
    # compiling would otherwise look older than its object file and be skipped.
    rsync -a --delete --exclude .git --exclude build --exclude demo --out-format='%n' "$SRC/" "$WORK/" > /tmp/changed.txt
    (cd "$WORK" && while IFS= read -r f; do [ -f "$f" ] && touch "$f"; done < /tmp/changed.txt)
    # Incremental: the Makefile tracks header dependencies and builds in parallel,
    # and /work is a volume, so only changed files recompile (even after restarts).
    if (cd "$WORK" && make CXX=g++ CC=gcc CFLAGS="-O1 -g" \
            CXXFLAGS="-I. -O1 -g $(sdl2-config --cflags)" \
            LDFLAGS="" \
            LIBS="-lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_mixer -ljson-c -lwebp -lz -lpthread -lm") \
            > "$LOGS/build.log" 2>&1; then
        # Keep the app's own state (theme config, image cache) across rebuilds.
        rsync -a --exclude config.json --exclude cache "$WORK/build/Plunder/" "$APP/"
        [ -f "$APP/config.json" ] || cp "$WORK/build/Plunder/config.json" "$APP/config.json"
        BUILD_STATE=ok
        echo "[demo] build #$BUILD_NO ok"
    else
        BUILD_STATE=failed
        echo "[demo] build #$BUILD_NO FAILED (see /logs/build.log)"; tail -20 "$LOGS/build.log"
    fi
    BUILD_TIME=$(date -Iseconds)
    BUILD_COMMIT=$(commit_label)
    write_status
    [ "$BUILD_STATE" = ok ]
}

start_app() {
    [ -x "$APP/Plunder" ] || return
    (cd "$APP" && exec env LD_PRELOAD=/usr/local/lib/libplunder_keys.so ./Plunder) > "$LOGS/app.log" 2>&1 &
    APP_PID=$!
    echo "[demo] app started (pid $APP_PID)"
    write_status
}

stop_app() {
    [ -n "$APP_PID" ] || return
    kill "$APP_PID" 2>/dev/null
    for _ in 1 2 3 4 5 6 7 8 9 10; do kill -0 "$APP_PID" 2>/dev/null || break; sleep 0.3; done
    kill -9 "$APP_PID" 2>/dev/null
    wait "$APP_PID" 2>/dev/null
    APP_PID=
}

shutdown() { echo "[demo] shutting down"; stop_app; kill $(jobs -p) 2>/dev/null; exit 0; }
trap shutdown TERM INT

# --- display + VNC + web ------------------------------------------------------
rm -f /tmp/.X0-lock /tmp/.X11-unix/X0
Xvfb :0 -screen 0 1280x720x24 -nolisten tcp >/dev/null 2>&1 &
for _ in $(seq 1 50); do [ -S /tmp/.X11-unix/X0 ] && break; sleep 0.1; done
x11vnc -display :0 -forever -shared -nopw -localhost -rfbport 5900 -quiet -noxdamage >/dev/null 2>&1 &
websockify --web "$WEB" 6080 localhost:5900 >/dev/null 2>&1 &

# --- build / run / watch loop ---------------------------------------------------
write_status
LAST_FP=$(fingerprint)
build; start_app

while true; do
    sleep "$POLL_SECONDS" & wait $!
    FP=$(fingerprint)
    if [ "$FP" != "$LAST_FP" ]; then
        # Debounce: wait until the tree stops changing (editor saves, git checkout).
        while sleep 1; NEXT=$(fingerprint); [ "$NEXT" != "$FP" ]; do FP=$NEXT; done
        LAST_FP=$FP
        echo "[demo] source change detected"
        if build; then stop_app; start_app; fi
        continue
    fi
    if [ -n "$APP_PID" ] && ! kill -0 "$APP_PID" 2>/dev/null; then
        echo "[demo] app exited; restarting"
        wait "$APP_PID" 2>/dev/null; APP_PID=
        sleep 1; start_app
    elif [ -z "$APP_PID" ] && [ "$BUILD_STATE" = ok ]; then
        start_app
    fi
    write_status
done
