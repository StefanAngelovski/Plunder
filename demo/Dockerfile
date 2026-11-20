# Plunder desktop demo: builds the app natively (x86_64) from the mounted repo,
# runs it on a virtual 1280x720 display and serves it over noVNC with an
# on-screen gamepad. The repo is NOT baked in; it is mounted read-only at /src
# and rebuilt automatically when sources change (see entrypoint.sh).
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential pkg-config \
        libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev libjson-c-dev libwebp-dev zlib1g-dev \
        xvfb x11vnc novnc websockify libgl1-mesa-dri \
        curl wget ca-certificates unzip 7zip rsync git procps \
    && rm -rf /var/lib/apt/lists/*

# Keyboard -> gamepad translation, injected into the app with LD_PRELOAD.
COPY keymap_shim.c /opt/plunder-demo/keymap_shim.c
RUN gcc -O2 -shared -fPIC $(sdl2-config --cflags) \
        -o /usr/local/lib/libplunder_keys.so /opt/plunder-demo/keymap_shim.c -ldl

# Web front-end: our page + noVNC's client library.
COPY web/ /web/
RUN cp -r /usr/share/novnc/core /usr/share/novnc/vendor /web/ \
    && git config --global --add safe.directory '*'

COPY entrypoint.sh /usr/local/bin/plunder-demo
RUN chmod +x /usr/local/bin/plunder-demo

EXPOSE 6080
CMD ["/usr/local/bin/plunder-demo"]
