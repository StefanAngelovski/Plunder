// LD_PRELOAD shim for the desktop demo.
//
// Plunder reads input almost exclusively as SDL game-controller events, and the
// demo has no controller. This wraps SDL_PollEvent and rewrites mapped keyboard
// events into SDL_CONTROLLERBUTTONDOWN/UP, so the app runs unmodified.
//
// Mapping (named after the *app's* buttons; see src/app/ControllerButtons.h,
// which swaps SDL's A/B and X/Y to match the Trimui's Nintendo-style layout):
//   Arrows                     -> D-pad
//   Enter / A / K              -> A  (confirm)
//   Backspace / Esc / B / J    -> B  (back)
//   X / I                      -> X
//   Y / U                      -> Y  (filter / search)
//   Q / PageUp                 -> L shoulder
//   E / PageDown               -> R shoulder
#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <string.h>

static int (*real_poll)(SDL_Event *) = NULL;

static int map_key(SDL_Keycode k) {
    switch (k) {
        case SDLK_UP:    return SDL_CONTROLLER_BUTTON_DPAD_UP;
        case SDLK_DOWN:  return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        case SDLK_LEFT:  return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
        case SDLK_RIGHT: return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;

        // app BUTTON_A == SDL_CONTROLLER_BUTTON_B
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_a: case SDLK_k:
            return SDL_CONTROLLER_BUTTON_B;
        // app BUTTON_B == SDL_CONTROLLER_BUTTON_A
        case SDLK_BACKSPACE: case SDLK_ESCAPE: case SDLK_b: case SDLK_j:
            return SDL_CONTROLLER_BUTTON_A;
        // app BUTTON_X == SDL_CONTROLLER_BUTTON_Y
        case SDLK_x: case SDLK_i:
            return SDL_CONTROLLER_BUTTON_Y;
        // app BUTTON_Y == SDL_CONTROLLER_BUTTON_X
        case SDLK_y: case SDLK_u:
            return SDL_CONTROLLER_BUTTON_X;

        case SDLK_q: case SDLK_PAGEUP:   return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        case SDLK_e: case SDLK_PAGEDOWN: return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        default: return -1;
    }
}

int SDL_PollEvent(SDL_Event *event) {
    if (!real_poll) real_poll = (int (*)(SDL_Event *))dlsym(RTLD_NEXT, "SDL_PollEvent");
    int got = real_poll(event);
    if (got && event && (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)) {
        int button = map_key(event->key.keysym.sym);
        if (button >= 0) {
            int down = event->type == SDL_KEYDOWN;
            Uint32 ts = event->key.timestamp;
            memset(event, 0, sizeof(*event));
            event->type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
            event->cbutton.timestamp = ts;
            event->cbutton.which = 0;
            event->cbutton.button = (Uint8)button;
            event->cbutton.state = down ? SDL_PRESSED : SDL_RELEASED;
        }
    }
    return got;
}
