# 01 — The frame

Every real-time program is this loop:

```cpp
while (running) {
    read input;          // what did the user do since last time?
    simulate(dt);        // advance the world by dt seconds
    render();            // draw one picture of it
    present();           // hand that picture to the display
}
```

`src/main.cpp` is exactly that, with the four steps labelled. If you ever feel lost in
the codebase, come back here: everything is reachable from one of those four lines.

## Delta time, and the bug it prevents

The naive version of movement is `position += speed` once per frame. It is wrong, and
the symptom is strange: the game runs at double speed on a 120 Hz monitor and in slow
motion when the frame rate drops.

The fix is to measure how long the previous frame took and scale by it:

```cpp
position += velocity * dt;   // dt in seconds
```

Now "4.6 blocks per second" means 4.6 blocks per second on any machine.
`src/core/Clock.h` measures `dt` with `SDL_GetPerformanceCounter`, which is a
high-resolution monotonic counter — not the wall clock, which can jump when the system
clock is adjusted.

## Why `dt` is clamped

```cpp
static constexpr float kMaxDelta = 0.1f;
if (dt > kMaxDelta) dt = kMaxDelta;
```

Drag the window, hit a chunk-loading hitch, or let the laptop sleep, and one frame might
take 2 seconds. With gravity at 28 m/s², a 2-second step moves the player 56 blocks
downwards *in a single step* — straight through the floor, because collision only
checks the blocks the box ends up overlapping, not the ones it flew past.

Clamping converts "a huge step" into "time briefly runs slow", which nobody notices and
nothing breaks. The grown-up version of this fix is a **fixed timestep**: accumulate
real time and run the simulation in constant 1/60 s slices. That is roadmap item 6 in
[06-roadmap.md](06-roadmap.md); the clamp is the 90% version in one line.

## Vsync

`SDL_GL_SetSwapInterval(1)` asks the driver to present frames in step with the monitor's
refresh. Two consequences:

- **No tearing.** The display never shows half of one frame and half of the next.
- **The loop is paced for you.** `SDL_GL_SwapWindow` blocks until the monitor is ready,
  so the program idles instead of spinning the GPU at 900 fps to no visible benefit.

With vsync on, your FPS readout is capped at the refresh rate, so it stops being a
useful performance measure. To see the real cost of a change, either turn the swap
interval to 0 or watch the triangle and chunk counts instead.

## Input: held vs. pressed

`src/core/Input.h` answers two different questions, and mixing them up causes bugs that
look like "the key is too sensitive":

| Query | Means | Use for |
|---|---|---|
| `keyDown(W)` | held right now | movement — you want it every frame |
| `keyPressed(F)` | went down *this frame* | toggles — you want it exactly once |

If flying were toggled on `keyDown(F)`, a 0.2 s tap at 120 fps would flip it 24 times.
`beginFrame()` clears the "pressed" set each frame, so an edge is reported once.

Mouse look uses `xrel`/`yrel` (relative motion) rather than cursor position, because the
cursor is locked: there is no position to read, only deltas. That is what
`SDL_SetWindowRelativeMouseMode` gives us.

## Exercises

1. Print `dt` for a few seconds. Drag the window and watch it spike — then raise
   `kMaxDelta` to 2.0 and jump off a cliff while dragging.
2. Set the swap interval to 0 and compare the FPS readout. Is anything else different?
3. Add a pause key that skips the simulate step but keeps rendering. What happens to
   `dt` on the frame you unpause — and why does the clamp save you again?
